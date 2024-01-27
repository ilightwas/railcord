
#include <algorithm>
#include <functional>
#include <future>
#include <memory>
#include <time.h>
#include <vector>

#include <cpr/cpr.h>

#include "alert_manager.h"
#include "gamedata.h"
#include "logger.h"
#include "lucyapi.h"
#include "personality_watcher.h"
#include "util.h"

namespace railcord {

using namespace std::chrono;
using json = nlohmann::json;

/// ---------------------------------------- PUBLIC ---------------------------------------
#pragma region PUBLIC

personality_watcher::personality_watcher(dpp::cluster* bot, Gamedata* g, Alert_Manager* al_mn)
    : bot_(bot), gamedata(g), alert_manager_(al_mn), watching_(false), sent_msgs_(bot) {}

personality_watcher::~personality_watcher() {
    watching_.store(false);

    if (personality_thread_.joinable()) {
        cv_.notify_one();
        personality_thread_.join();
    }
}

void personality_watcher::run() {
    std::lock_guard<std::mutex> lock{mtx_};
    if (!watching_.load()) {
        logger->debug("Starting personality watcher thread");
        watching_.store(true);
        personality_thread_ = std::thread(&personality_watcher::personality_update, this);
    }
}

void personality_watcher::stop() {
    std::unique_lock<std::mutex> lock{mtx_};
    if (watching_.load()) {
        logger->debug("Stopping personality watcher thread");
        watching_.store(false);
        if (personality_thread_.joinable()) {
            cv_.notify_one();
            lock.unlock();   // avoid dead lock, the other thread might try locking this mutex
            personality_thread_.join();
        }
    }
}

bool personality_watcher::is_watching() {
    std::lock_guard<std::mutex> lock{mtx_};
    return watching_.load();
}

#pragma endregion PUBLIC

/// ---------------------------------------- PRIVATE ---------------------------------------
#pragma region PRIVATE

void personality_watcher::personality_update() {
    logger->debug("Personality thread started");

    if (use_local_time_ || !sync_time()) {
        logger->warn("Using local system time");
        do_sync_time(system_clock::now(), steady_clock::now());
    }

    while (watching_.load()) {
        if (errors_ > s_max_tries) {
            logger->warn("Failed to update personalities {} times, stopping thread..", s_max_tries);
            watching_.store(false);
            continue;
        }

        auto request_success = request_auctions();
        if (!request_success) {
            ++errors_;
            std::unique_lock<std::mutex> lock(mtx_);
            logger->warn("Update personalities failed before, waiting 30 seconds before next try..");
            cv_.wait_for(lock, seconds{30}, [this]() { return !watching_.load(); });
            continue;
        }

        try {
            process_auctions(*request_success);
        } catch (const std::exception& e) {
            logger->error("Something went wrong while processing auctions: {}", e.what());
            watching_.store(false);
            continue;
        }

        wait();
        alert_manager_->refresh_active_auctions();
    }

    reset();
    logger->debug("Personality thread finished");
}

std::optional<std::vector<auction>> personality_watcher::request_auctions() {
    cpr::Response r =
        cpr::Get(cpr::Url{api_endpoints.at(api::personality_id)}, cpr::Timeout{seconds{s_request_auction_timeout}});
    if (r.status_code != 200) {
        if (r.status_code == 0) {
            logger->warn("Personalities request timed out");
        } else {
            logger->warn("Personalities request failed with status code={}", r.status_code);
        }
        return {};
    }

    try {
        return nlohmann::json::parse(r.text).at("Body").at("Personalities").at("auctions").get<std::vector<auction>>();
    } catch (const json::exception& e) {
        logger->warn("Parsing json failed with: {}", e.what());
        return {};
    } catch (const std::exception& e) {
        logger->warn("request_personalities failed with: {}", e.what());
        return {};
    }

    // auto promise = std::make_shared<std::promise<nlohmann::json>>();
    // auto f = promise->get_future();

    // bot_->request(api_endpoints.at(api::personality_id), dpp::http_method::m_get,
    //               [promise](const dpp::http_request_completion_t& request) {
    //                   if (request.status == 200) {
    //                       try {
    //                           promise->set_value(nlohmann::json::parse(request.body));
    //                       } catch (const json::exception& e) {
    //                           promise->set_exception(std::make_exception_ptr(e));
    //                       }
    //                   } else {
    //                       promise->set_exception(std::make_exception_ptr(std::runtime_error{
    //                           fmt::format("Personality request failed with status {}", request.status)}));
    //                   }
    //               });

    // std::future_status status = f.wait_for(std::chrono::seconds{s_request_auction_timeout});

    // if (status != std::future_status::ready) {
    //     logger->warn("Personalities request timed out");
    //     return {};
    // }

    // try {
    //     return f.get().at("Body").at("Personalities").at("auctions").get<std::vector<auction>>();
    // } catch (const std::exception& e) {
    //     logger->warn("request_personalities failed with: {}", e.what());
    //     return {};
    // }
}

void personality_watcher::process_auctions(std::vector<auction>& auctions) {
    std::sort(auctions.begin(), auctions.end(),
              [](const auction& a, const auction& b) { return a.end_time < b.end_time; });

    const auto new_auctions = [&]() {
        std::vector<auction*> v;
        for (auto&& a : auctions) {
            bool inserted = alert_manager_->add_seen_auction_id(a.id);
            if (inserted) {   // new id
                v.push_back(&a);
                if (v.size() > 10) {   // embed max
                    logger->warn("Received more than 10 new auctions");
                    break;
                }
            }
        }
        return v;
    }();

    if (new_auctions.empty()) {
        // wait until next hour fifth minute
        auto left_to_next_hr = util::left_to_next_hour(system_clock::now()) + auction::s_request_wait;
        logger->info("No new auctions in the last request, shift next request by {}",
                     util::fmt_to_hr_min_sec(left_to_next_hr));

        if (wait_times_.empty()) {
            wait_times_.push_back(left_to_next_hr);
        } else {
            wait_times_.front() = left_to_next_hr;
        }

        return;
    }

    auto server_time = server_time_now();
    for (auto&& au : new_auctions) {
        active_auction new_active_auction{*au, server_time, &gamedata->get_personality(au->personality_id)};
        add_wait_time(&new_active_auction);
        alert_manager_->add_active_auction(new_active_auction);

        auto type = new_active_auction.p->info.ptype;
        if (active_only_horizon_msg_ && !alert_manager_->is_alert_enabled(type)) {
            continue;
        }

        dpp::message msg;
        msg.channel_id = alert_manager_->get_alert_channel();
        auto e = util::build_embed(new_active_auction.client_ends_at(), *new_active_auction.p, true);
        if (alert_manager_->has_horizon_message(type)) {
            e.add_field("", alert_manager_->get_horizon_message(type));
        }
        msg.add_embed(e);

        send_discord_msg(msg, new_active_auction.end_time_for_alert());
    }
}

bool personality_watcher::sync_time() {
    auto promise = std::make_shared<std::promise<std::uint64_t>>();
    auto f = promise->get_future();

    auto request_time = steady_clock::now();
    bot_->request(api_endpoints.at(api::sync_time_id), dpp::http_method::m_get,
                  [promise](const dpp::http_request_completion_t& request) {
                      if (request.status == 200) {
                          try {
                              auto j = nlohmann::json::parse(request.body);
                              std::uint64_t server_time = j.at("Body").get<std::uint64_t>();
                              promise->set_value(server_time);

                          } catch (const json::exception& e) {
                              promise->set_exception(std::make_exception_ptr(e));
                          }
                      } else {
                          promise->set_exception(std::make_exception_ptr(std::runtime_error{
                              fmt::format("Sync time request failed with status {}", request.status)}));
                      }
                  });

    std::future_status status = f.wait_for(seconds{s_sync_time_timeout});

    if (status != std::future_status::ready) {
        logger->warn("Sync time request timed out");
        return false;
    }

    try {
        do_sync_time(system_clock::time_point(seconds{f.get()}), request_time);
        return true;
    } catch (const std::exception& e) {
        logger->warn("Sync time failed with: {}", e.what());
        return false;
    }
}

void personality_watcher::do_sync_time(system_clock::time_point server_time, steady_clock::time_point request_time) {
    server_time_ = server_time;
    auto steady_now = steady_clock::now();
    auto system_now = system_clock::now();
    auto time_taken_by_request = std::chrono::abs(steady_now - request_time);
    at_sync_time_ = steady_now - time_taken_by_request;
    server_delta_ = duration_cast<milliseconds>(std::chrono::abs(system_now - server_time_) + time_taken_by_request);

    logger->debug("Time taken by request: {:.2f}s", duration_cast<duration<float>>(time_taken_by_request).count());
    logger->debug("Server delta: {:.2f}s", duration_cast<duration<float>>(server_delta_).count());
    logger->debug("Sync time finished");
}

void personality_watcher::add_wait_time(active_auction* au) {
    auto already_waited = std::accumulate(
        wait_times_.begin(), wait_times_.end(), system_clock::duration{0}, std::plus<system_clock::duration>{});
    auto to_wait = std::chrono::abs(au->end_time - already_waited + auction::s_request_wait);
    logger->debug("Add Wait: {} offset: {}, to wait {}", au->p->name, util::fmt_to_hr_min_sec(already_waited),
                  util::fmt_to_hr_min_sec(to_wait));
    wait_times_.push_back(to_wait);
}

void personality_watcher::wait() {
    std::unique_lock<std::mutex> lock(mtx_);

    const auto empty_waits = [&]() {
        auto to_wait = util::left_to_next_hour(system_clock::now()) + auction::s_request_wait;
        logger->debug("Waiting times were empty, waiting {} next hour", util::fmt_to_hr_min_sec(to_wait));
        return to_wait;
    };

    const auto& w = wait_times_.empty() ? empty_waits() : [&]() {
        auto&& r = wait_times_.front();
        wait_times_.pop_front();
        return r;
    }();

    logger->debug("Waiting: {}", util::fmt_to_hr_min_sec(w));
    cv_.wait_for(lock, w, [this]() { return !watching_.load(); });   // if not running on spurious wakeup stop waiting
}

system_clock::time_point personality_watcher::server_time_now() {
    return server_time_ + server_delta_ +
           duration_cast<system_clock::duration>(std::chrono::abs(steady_clock::now() - at_sync_time_));
}

void personality_watcher::send_discord_msg(const dpp::message& msg, system_clock::duration wait_delete) {
    bot_->message_create(msg, [this, wait_delete](const dpp::confirmation_callback_t& cc) {
        if (cc.is_error()) {
            logger->warn("Bot failed to create personality message: {}", cc.get_error().message);
            return;
        }

        const dpp::message& m = cc.get<dpp::message>();
        sent_msgs_.add_message(m.id, m.channel_id);
        util::one_shot_timer(
            bot_, [msg_id = m.id, s = &sent_msgs_]() { s->delete_message(msg_id, "(non alert)"); },
            static_cast<uint64_t>(duration_cast<seconds>(wait_delete).count()) + util::s_delete_message_delay);
    });
}

void personality_watcher::reset() {
    wait_times_.clear();
    alert_manager_->reset_alerts();
    sent_msgs_.delete_all_messages(true);
    errors_ = 0;
}

#pragma endregion PRIVATE

}   // namespace railcord
