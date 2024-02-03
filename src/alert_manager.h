#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include <dpp/dpp.h>

#include "alert_info.h"
#include "message_tracker.h"
#include "personality.h"

namespace railcord {

inline constexpr const char* s_alert_manager_file{"state.json"};

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

    std::string get_alert_message(personality::type t);
    void set_alert_message(personality::type t, const std::string& msg);
    bool has_alert_message(personality::type t);

    std::string get_horizon_message(personality::type t);
    void set_horizon_message(personality::type t, const std::string& msg);
    bool has_horizon_message(personality::type t);

    dpp::snowflake get_alert_role();
    void set_alert_role(dpp::snowflake role);

    dpp::snowflake get_alert_channel();
    void set_alert_channel(dpp::snowflake channel);

    std::vector<Alert_Info> get_alerts_info();
    void set_alerts_info(std::vector<Alert_Info> alerts_info);

    void add_custom_message(const std::string& title, const std::string& msg);
    void remove_custom_message(int id);

    std::vector<Custom_Message> get_custom_msgs();
    void set_custom_msgs(std::vector<Custom_Message> custom_msgs);

    bool set_custom_msg_for_alert(personality::type t, int id);
    bool set_custom_msg_for_horizon(personality::type t, int id);
    bool has_custom_msgs();

    bool load_state();
    bool save_state();

    void reset_alerts();
    void refresh_active_auctions();

  private:
    Alert_Info& get_alert_by_type(personality::type t);
    void add_timer(const std::string& id, int interval, dpp::timer timer);
    void stop_timers(const std::string& id);
    void stop_timers(personality::type t);
    void update_alerts(personality::type t);

    std::shared_mutex mtx_;
    dpp::cluster* bot_;

    std::vector<Alert_Info> alerts_info_;
    std::vector<active_auction> active_auctions_;
    std::unordered_set<std::string> seen_auction_ids_;

    dpp::snowflake alert_role_;
    dpp::snowflake alert_channel_;
    MessageTracker sent_msgs_;
    std::vector<Custom_Message> custom_msgs_;
};

}   // namespace railcord

#endif   // !ALERT_MANAGER_H
