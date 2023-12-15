#ifndef ALERT_INFO_H
#define ALERT_INFO_H

#include <string>
#include <vector>

#include "personality.h"

namespace railcord {

class Alert_Info {
  public:
    Alert_Info(personality::type t);

    const std::string& msg() const { return msg_; }
    void set_msg(const std::string& msg) { msg_ = msg; }
    bool has_custom_msg() { return !msg_.empty(); }

    bool is_enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }

    void enable_interval(int interval);
    void disable_interval(int interval);
    bool interval_is_enabled(int interval);
    const std::vector<int>& intervals() { return enabled_intervals_; }

    personality::type type() { return type_; }

    static const std::vector<int> s_intervals;

  private:
    bool enabled_{false};
    std::string msg_;
    personality::type type_;
    std::vector<int> enabled_intervals_;
};

struct alert_data {
    alert_data(uint64_t seconds, int interval, const dpp::message& msg)
        : seconds(seconds), interval(interval), msg(msg) {}
    uint64_t seconds;
    int interval;
    const dpp::message msg;
};

}   // namespace railcord

#endif   // !ALERT_INFO_H
