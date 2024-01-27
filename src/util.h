#ifndef UTIL_H
#define UTIL_H

#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <time.h>
#include <vector>

#include <dpp/dpp.h>
#include <dpp/nlohmann/json.hpp>

#include "alert_info.h"
#include "license.h"
#include "logger.h"
#include "personality.h"

namespace railcord {
class Gamedata;
struct Good;
struct License;
struct Embed_Data;
class Sent_Messages;
}   // namespace railcord

namespace railcord::util {

inline constexpr auto s_inv_space = "‎";
inline constexpr uint64_t s_delete_message_delay = 180;

std::tm get_localtime(std::time_t* tt);
std::chrono::system_clock::duration left_to_next_hour(std::chrono::system_clock::time_point tp);

std::string get_token(const std::string& token_file);
std::string md5(const std::string& str);

dpp::timer make_alert(dpp::cluster* bot, const Alert_Data& data, Sent_Messages* sent_msgs);
dpp::embed build_embed(std::chrono::system_clock::time_point tp, const personality& p, bool with_timer = false);
dpp::message build_license_msg(License::Embed_Data* eb);

std::string fmt_http_request(const std::string& server, int port, const std::string& endpoint, bool https = false);
uint32_t rnd_color();
std::string rnd_emoji(uint32_t idx = 0);
uint32_t rnd_gen(std::function<uint32_t(std::mt19937& gen)> f);

template <typename Tp>
std::string timepoint_to_discord_timestamp(Tp tp, const char* fmt = "R") {
    using namespace std::chrono;

    // <t:1698449700:R>
    return std::string{}
        .append("<t:")
        .append(std::to_string(duration_cast<seconds>(tp.time_since_epoch()).count()))
        .append(":")
        .append(fmt)
        .append(">");
}

template <typename Tp>
std::string timepoint_to_date(Tp tp) {
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{get_localtime(&tt)};

    char buff[1024];

    // not thread safe
    if (strftime(buff, sizeof buff, "%I:%M:%S %p", &tm)) {
        return buff;
    }

    return {};
}

template <typename Duration>
std::string fmt_to_hr_min_sec(Duration d) {
    using namespace std::chrono;
    std::string tmp;

    bool has_hours = false;
    if (d > hours{1}) {
        hours h = duration_cast<hours>(d);
        d -= h;
        tmp.append(std::to_string(h.count()));
        tmp.append("h");
        has_hours = true;
    }

    bool has_minutes = false;
    if (d > minutes{1}) {
        minutes m = duration_cast<minutes>(d);
        d -= m;

        tmp.append(std::to_string(m.count()));
        tmp.append("m");
        has_minutes = true;
    }

    if (d > seconds{1}) {
        tmp.append(std::to_string(duration_cast<seconds>(d).count()));
        tmp.append("s");
    } else if (!has_hours && !has_minutes) {
        tmp.append("0s");
    }

    return tmp;
}

inline std::string user_mention(dpp::snowflake user) {
    return std::string{}.append("<@").append(std::to_string(user)).append(">");
}

inline dpp::timer one_shot_timer(dpp::cluster* bot, std::function<void()> f, uint64_t seconds) {
    return bot->start_timer(
        [bot, f](dpp::timer t) {
            f();
            bot->stop_timer(t);
        },
        seconds);
}

inline int as_int(const nlohmann::json& j) { return std::stoi(j.get<std::string>()); }
inline bool starts_with(const std::string& s, const std::string& prefix) { return (s.rfind(prefix, 0) == 0); }
inline std::string to_lowercase(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

}   // namespace railcord::util

#endif   // !UTIL_H
