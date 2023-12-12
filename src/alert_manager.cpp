#include <algorithm>
#include <chrono>

#include <fmt/format.h>

#include "alert_manager.h"
#include "gamedata.h"
#include "logger.h"
#include "util.h"

namespace railcord {

using namespace std::chrono;

/// ---------------------------------------- PUBLIC ---------------------------------------
#pragma region PUBLIC

Alert_Manager::Alert_Manager(dpp::cluster* bot) : bot_(bot) {
    for (uint8_t type = personality::type::goods; type < personality::type::unknown; ++type) {
        alerts_.emplace_back(type);
    }
}

void Alert_Manager::add_active_auction(const active_auction& au) {
    std::unique_lock<std::shared_mutex> lock{mtx_};
    active_auctions_.push_back(au);
    auto time_left = std::chrono::abs(au.ends_at - system_clock::now());
    logger->debug("Time left: {} {}", util::fmt_to_hr_min_sec(time_left), au.p->name);

    Alert_Info& alert = get_alert_by_type(au.p->info.ptype);
    if (!alert.is_enabled()) {
        return;
    }

    update_alerts(au.p->info.ptype);
}

bool Alert_Manager::add_seen_auction_id(const std::string& id) {
    std::unique_lock<std::shared_mutex> lock{mtx_};
    auto [iter, inserted] = seen_auction_ids_.insert(id);
    return inserted;
}

void Alert_Manager::set_alert_enabled(personality::type t, bool enabled) {
    std::unique_lock<std::shared_mutex> lock{mtx_};
    Alert_Info& alert = get_alert_by_type(t);

    if (alert.is_enabled() == enabled) {
        logger->debug("Current alert type {} is already {}", t.t, enabled ? "enabled" : "disabled");
        return;
    }

    logger->debug("{} alert for type {}", enabled ? "Enabling" : "Disabling", t.t);
    alert.set_enabled(enabled);

    update_alerts(t);
}

void Alert_Manager::set_alert_interval(personality::type t, int interval, bool enabled) {
    std::unique_lock<std::shared_mutex> lock{mtx_};
    Alert_Info& alert = get_alert_by_type(t);
    if (alert.interval_is_enabled(interval) == enabled) {
        return;
    }

    if (enabled) {
        alert.enable_interval(interval);
    } else {
        alert.disable_interval(interval);
    }

    update_alerts(t);
}

bool Alert_Manager::is_alert_enabled(personality::type t) {
    std::shared_lock<std::shared_mutex> lock{mtx_};
    return get_alert_by_type(t).is_enabled();
}

bool Alert_Manager::is_interval_enabled(personality::type t, int interval) {
    std::shared_lock<std::shared_mutex> lock{mtx_};
    return get_alert_by_type(t).interval_is_enabled(interval);
}

dpp::message Alert_Manager::build_alert_message(const personality& p, system_clock::time_point ends_at,
                                                const std::string& custom_msg, int interval) {
    dpp::embed e = util::build_embed(ends_at, p);
    e.set_color(util::rnd_color());

    if (!custom_msg.empty()) {
        e.add_field("", custom_msg);
    }

    dpp::message m;
    m.content
        .append(
            fmt::format("{} {} left to deadline\n{} worker", interval, interval == 1 ? "minute" : "minutes", p.info.ptype.description()))
        .append("\n")
        .append(util::timepoint_to_discord_timestamp(ends_at, "T"))
        .append("\n**## ")
        .append(util::timepoint_to_discord_timestamp(ends_at))
        .append("**")
        .append(fmt::format("<@&{}>", static_cast<uint64_t>(alert_role_)));
    m.set_channel_id(alert_channel_);
    m.add_embed(e);
    m.allowed_mentions.parse_everyone = true;
    m.allowed_mentions.parse_roles = true;

    m.set_flags(dpp::m_ephemeral);
    return m;
}

const std::string& Alert_Manager::get_alert_message(personality::type t) {
    std::shared_lock<std::shared_mutex> lock{mtx_};
    return get_alert_by_type(t).msg();
}

void Alert_Manager::set_alert_message(personality::type t, const std::string& msg) {
    std::unique_lock<std::shared_mutex> lock{mtx_};
    get_alert_by_type(t).set_msg(msg);
}

dpp::snowflake Alert_Manager::get_alert_role() {
    std::shared_lock<std::shared_mutex> lock{mtx_};
    return alert_role_;
}

void Alert_Manager::set_alert_role(dpp::snowflake role) {
    std::unique_lock<std::shared_mutex> lock{mtx_};
    alert_role_ = role;
}

dpp::snowflake Alert_Manager::get_alert_channel() {
    std::shared_lock<std::shared_mutex> lock{mtx_};
    return alert_channel_;
}

void Alert_Manager::set_alert_channel(dpp::snowflake channel) {
    std::unique_lock<std::shared_mutex> lock{mtx_};
    alert_channel_ = channel;
}

void Alert_Manager::reset_alerts() {
    std::unique_lock<std::shared_mutex> lock{mtx_};
    for (auto& au : active_auctions_) {
        stop_timers(au.id);
    }

    active_auctions_.clear();
    seen_auction_ids_.clear();
}

void Alert_Manager::refresh_active_auctions() {
    std::unique_lock<std::shared_mutex> lock{mtx_};
    active_auctions_.erase(std::remove_if(active_auctions_.begin(), active_auctions_.end(),
                                          [](active_auction& au) { return au.has_ended(); }),
                           active_auctions_.end());
}

#pragma endregion PUBLIC

/// ---------------------------------------- PRIVATE ---------------------------------------
#pragma region PRIVATE

Alert_Info& Alert_Manager::get_alert_by_type(personality::type t) {
    if (t < personality::type::unknown) {
        return alerts_[t];
    }
    throw std::invalid_argument{fmt::format("Invalid personality type \"{}\" for indexing alerts", t.t)};
}

void Alert_Manager::add_timer(const std::string& id, int interval, dpp::timer timer) {
    auto it =
        std::find_if(active_auctions_.begin(), active_auctions_.end(), [&](const auto& au) { return au.id == id; });

    if (it != active_auctions_.end()) {
        it->timers_.emplace(interval, timer);
    }
}

void Alert_Manager::stop_timers(const std::string& id) {
    auto it =
        std::find_if(active_auctions_.begin(), active_auctions_.end(), [&](const auto& au) { return au.id == id; });
    if (it == active_auctions_.end()) {
        return;
    }

    active_auction& ac_auction = *it;
    for (auto& [interval, timer] : ac_auction.timers_) {
        bot_->stop_timer(timer);
    }

    ac_auction.timers_.clear();
}

void Alert_Manager::update_alerts(personality::type t) {
    for (auto& ac_auction : active_auctions_) {
        if (ac_auction.has_ended() || ac_auction.p->info.ptype != t) {
            continue;
        }

        Alert_Info& alert = get_alert_by_type(t);
        for (int interval : Alert_Info::s_intervals) {
            if (alert.interval_is_enabled(interval) && alert.is_enabled()) {
                if (ac_auction.has_interval_timer(interval)) {
                    continue;
                } else {
                    if (ac_auction.expired_for_interval(interval)) {
                        continue;   // alert expired
                    }

                    auto delay = ac_auction.wait_delay_for_interval(interval);
                    logger->debug("Alert in: {} {}", util::fmt_to_hr_min_sec(delay), ac_auction.p->name);

                    add_timer(
                        ac_auction.id, interval,
                        util::make_alert(
                            bot_, static_cast<uint64_t>(duration_cast<seconds>(delay).count()), interval,
                            build_alert_message(*ac_auction.p, ac_auction.client_ends_at(), alert.msg(), interval)));
                }
            } else {
                if (!ac_auction.has_interval_timer(interval)) {
                    continue;
                } else {
                    logger->debug("Disabling alert for interval {} for {}", interval, t.t);
                    auto it = ac_auction.timers_.find(interval);
                    bot_->stop_timer(it->second);
                    ac_auction.timers_.erase(it);
                }
            }
        }
    }
}

#pragma endregion PRIVATE

}   // namespace railcord
