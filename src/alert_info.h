#ifndef ALERT_INFO_H
#define ALERT_INFO_H

#include <string>
#include <vector>

#include <dpp/nlohmann/json.hpp>

#include "personality.h"

namespace railcord {

class Alert_Info {
  public:
    Alert_Info(personality::type t);
    Alert_Info() = default;

    const std::string& msg() const { return msg_; }
    void set_msg(const std::string& msg) { msg_ = msg; }

    const std::string& horizon_msg() const { return horizon_msg_; }
    void set_horizon_msg(const std::string& msg) { horizon_msg_ = msg; }

    bool is_enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }

    void enable_interval(int interval);
    void disable_interval(int interval);
    bool interval_is_enabled(int interval);
    const std::vector<int>& intervals() { return enabled_intervals_; }

    personality::type type() const { return type_; }
    void set_type(personality::type t) { type_ = t; }

    static const std::vector<int> s_intervals;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Alert_Info, enabled_, msg_, horizon_msg_, type_, enabled_intervals_)

  private:
    bool enabled_{false};
    std::string msg_;
    std::string horizon_msg_;
    personality::type type_;
    std::vector<int> enabled_intervals_;
};

struct Alert_Data {
    Alert_Data(uint64_t seconds, int interval, const dpp::message& msg)
        : seconds(seconds), interval(interval), msg(msg) {}
    uint64_t seconds;
    int interval;
    const dpp::message msg;
};

struct Custom_Message {
    Custom_Message(int id, std::string title, std::string body)
        : id(id), title(std::move(title)), body(std::move(body)) {}
    Custom_Message() = default;

    int id;
    std::string title;
    std::string body;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Custom_Message, id, title, body)
};

}   // namespace railcord

#endif   // !ALERT_INFO_H
