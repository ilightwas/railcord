#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include <dpp/dpp.h>

#include "alert_info.h"
#include "personality.h"
#include "sent_messages.h"

namespace railcord {

class Alert_Manager {
  public:
    Alert_Manager(dpp::cluster* bot);
    Alert_Manager(const Alert_Manager&) = delete;
    Alert_Manager(Alert_Manager&&) = delete;
    Alert_Manager& operator=(const Alert_Manager&) = delete;
    Alert_Manager& operator=(Alert_Manager&&) = delete;

    void add_active_auction(const active_auction& au);
    bool add_seen_auction_id(const std::string& id);

    void set_alert_enabled(personality::type t, bool enabled);
    void set_alert_interval(personality::type t, int interval, bool enabled);
    bool is_alert_enabled(personality::type t);
    bool is_interval_enabled(personality::type, int interval);

    dpp::message build_alert_message(const personality& p, std::chrono::system_clock::time_point ends_at,
                                     const std::string& custom_msg, int interval);

    const std::string& get_alert_message(personality::type t);
    void set_alert_message(personality::type t, const std::string& msg);

    dpp::snowflake get_alert_role();
    void set_alert_role(dpp::snowflake role);

    dpp::snowflake get_alert_channel();
    void set_alert_channel(dpp::snowflake channel);

    void reset_alerts();
    void refresh_active_auctions();

  private:
    Alert_Info& get_alert_by_type(personality::type t);
    void add_timer(const std::string& id, int interval, dpp::timer timer);
    void stop_timers(const std::string& id);
    void update_alerts(personality::type t);

    std::shared_mutex mtx_;
    dpp::cluster* bot_;

    std::vector<Alert_Info> alerts_;
    std::vector<active_auction> active_auctions_;
    std::unordered_set<std::string> seen_auction_ids_;

    dpp::snowflake alert_role_;
    dpp::snowflake alert_channel_;
    sent_messages sent_msgs_;
};

}   // namespace railcord

#endif   // !ALERT_MANAGER_H
