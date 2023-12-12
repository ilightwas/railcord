
#include <algorithm>
#include <future>
#include <memory>
#include <vector>

#include "alert_manager.h"
#include "gamedata.h"
#include "logger.h"
#include "lucyapi.h"
#include "personality_watcher.h"
#include "util.h"

namespace railcord {

using namespace std::chrono;

/// ---------------------------------------- PUBLIC ---------------------------------------
#pragma region PUBLIC

personality_watcher::personality_watcher(dpp::cluster* bot, Gamedata* g, Alert_Manager* al_mn)
    : bot_(bot), gamedata(g), alert_manager_(al_mn), watching_(false) {}

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
            lock.lock();
        }
    }
}

#pragma endregion PUBLIC

/// ---------------------------------------- PRIVATE ---------------------------------------
#pragma region PRIVATE

void personality_watcher::personality_update() {
    logger->debug("Personality thread started");
    auto tries = std::make_shared<std::atomic<int>>(0);

    if (use_local_time_ || !sync_time()) {
        logger->warn("Using local system time");
        do_sync_time(system_clock::now(), steady_clock::now());
    }

    while (watching_.load()) {
        if (int tmp_tries = (*tries) != 0) {
            if (tmp_tries > s_max_tries) {
                logger->warn("Failed to update personalities {} times, stopping thread..", s_max_tries);
                watching_.store(false);
                continue;
            } else {
                logger->warn("Update personalities failed before, waiting 30 seconds before next try..");
                std::this_thread::sleep_for(seconds{30});
            }
        }

        auto request_success = request_auctions();
        if (!request_success) {
            ++(*tries);
            continue;
        }

        try {
            auto&& auctions = *request_success;
            std::sort(auctions.begin(), auctions.end(),
                      [](const auction& a, const auction& b) { return a.end_time < b.end_time; });

            std::vector<auction*> new_auctions{};
            for (auto&& a : auctions) {
                bool inserted = alert_manager_->add_seen_auction_id(a.id);
                if (inserted) {   // new id
                    new_auctions.push_back(&a);
                    if (new_auctions.size() > 10) {   // embed max
                        logger->warn("Received more than 10 new auctions");
                        break;
                    }
                }
            }

            if (!(new_auctions.size() > 0)) {
                logger->debug("No new auctions in the last request");
                return;
            }

            auto server_time = server_time_now();
            for (auto&& au : new_auctions) {
                update_wait_times(au);

                active_auction new_active_auction{*au, server_time, &gamedata->get_personality(au->personality_id)};
                alert_manager_->add_active_auction(new_active_auction);

                dpp::message msg;
                msg.channel_id = alert_manager_->get_alert_channel();
                msg.add_embed(util::build_embed(new_active_auction.client_ends_at(), *new_active_auction.p, true));

                send_discord_msg(msg, tries,
                                 std::chrono::abs(new_active_auction.end_time - auction::s_discord_extra_delay));
            }

        } catch (const std::exception& e) {
            logger->error("Something went wrong trying to send the discord message: {}", e.what());
            ++(*tries);
            continue;
        }

        wait();
        alert_manager_->refresh_active_auctions();
    }

    reset();
    logger->debug("Personality thread finished");
}

std::optional<std::vector<auction>> personality_watcher::request_auctions() {
    auto promise = std::make_shared<std::promise<nlohmann::json>>();

    auto f = promise->get_future();

    bot_->request(api_endpoints.at(api::personality_id), dpp::http_method::m_get,
                  [promise](const dpp::http_request_completion_t& request) {
                      if (request.status == 200) {
                          try {
                              promise->set_value(nlohmann::json::parse(request.body));
                          } catch (const json::exception& e) {
                              promise->set_exception(std::make_exception_ptr(e));
                          }
                      } else {
                          promise->set_exception(std::make_exception_ptr(std::runtime_error{
                              fmt::format("Personality request failed with status {}", request.status)}));
                      }
                  });

    std::future_status status = f.wait_for(std::chrono::seconds{s_request_auction_timeout});

    if (status != std::future_status::ready) {
        logger->warn("Personalities request request timed out");
        return {};
    }

    try {
        return f.get().at("Body").at("Personalities").at("auctions").get<std::vector<auction>>();
    } catch (const std::exception& e) {
        logger->warn("request_personalities failed with: {}", e.what());
        return {};
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

    logger->debug("time taken by request: {:.2f}s", duration_cast<duration<float>>(time_taken_by_request).count());
    logger->debug("Server delta: {:.2f}s", duration_cast<duration<float>>(server_delta_).count());
    logger->debug("Sync time finished");
}

void personality_watcher::update_wait_times(auction* au) {
    if (wait_times_.empty()) {
        wait_times_.push_back(au->end_time + auction::s_pause + auction::s_request_wait);
    } else {
        wait_times_.push_back(
            std::chrono::abs(au->end_time - std::accumulate(wait_times_.begin(), wait_times_.end(), seconds{0}) +
                             auction::s_pause + auction::s_request_wait));
    }
}

void personality_watcher::wait() {
    std::unique_lock<std::mutex> lock(mtx_);
    const auto& w = wait_times_.empty() ? minutes{10} : [&]() {
        auto&& r = wait_times_.front();
        wait_times_.pop_front();
        return r;
    }();

    logger->debug("Waiting: {}", util::fmt_to_hr_min_sec(w));
    cv_.wait_for(lock, w, [this]() { return !watching_.load(); });   // if not running on spurious wakeup stop waiting
    logger->debug("Finished waiting");
}

system_clock::time_point personality_watcher::server_time_now() {
    return server_time_ + server_delta_ +
           duration_cast<system_clock::duration>(std::chrono::abs(steady_clock::now() - at_sync_time_));
}

void personality_watcher::send_discord_msg(const dpp::message& msg, std::shared_ptr<std::atomic<int>>& last_tries,
                                           system_clock::duration wait_delete) {
    bot_->message_create(
        msg, [w_ptr = std::weak_ptr{last_tries}, wait_delete, bot = bot_](const dpp::confirmation_callback_t& cc) {
            auto tries = w_ptr.lock();

            if (cc.is_error()) {
                logger->warn("Bot failed to create personality message: {}", cc.get_error().message);
                if (tries) {
                    tries->fetch_add(1);
                }
                return;
            }

            if (tries) {
                int tmp_tries = tries->load();
                if (tmp_tries > 0) {
                    tries->fetch_sub(1);
                }
            }

            const dpp::message& m = cc.get<dpp::message>();
            bot->start_timer(
                [msg_id = m.id, ch_id = m.channel_id, bot](dpp::timer timer) {
                    logger->info("Deleting msg id = {} (non alert)", static_cast<uint64_t>(msg_id));
                    bot->message_delete(msg_id, ch_id);
                    bot->stop_timer(timer);
                },
                static_cast<uint64_t>(duration_cast<seconds>(wait_delete).count()) + util::s_delete_message_delay);
        });
}

void personality_watcher::reset() {
    wait_times_.clear();
    alert_manager_->reset_alerts();
}

#pragma endregion PRIVATE

}   // namespace railcord
