#ifndef PERSONALITY_H
#define PERSONALITY_H

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <dpp/dpp.h>
#include <dpp/nlohmann/json.hpp>

namespace railcord {

class Gamedata;
struct personality {
    struct type {
        enum : uint8_t {
            goods = 0,
            speed = 1,
            engine_upgrades = 2,
            servicing = 3,
            prestige_city = 4,
            cheaper_routes = 5,
            hourly = 6,
            station_faster_bonus = 7,
            station_construction_time = 8,
            station_cost = 9,
            waiting_time = 10,
            research = 11,
            investment = 12,
            city_connection = 13,
            railcars = 14,
            premium_gold = 15,
            acceleration = 16,
            competition = 17,
            prestige_facilities = 18,
            unknown = 19
        };

        type(uint8_t t) : t(t) {}
        type(int t) : t(static_cast<uint8_t>(t)) {}
        operator uint8_t() const { return t; }

        const char* description() const {
            static constexpr const char* desc[] = {
                "Goods bonus",
                "Increased engine speed",
                "Reduced engine upgrades cost",
                "Reduced cost of servicing",
                "Increased prestige city deliveries",
                "Cheaper track construction",
                "Hourly bonus of money or prestige",
                "Faster station bonus generation",
                "Reduced station construction time",
                "Reduced station construction cost",
                "Reduced waiting times",
                "Bonus research points",
                "Reduced investment cost",
                "Reduced city connection cost",
                "Cheaper goods waggons",
                "Reduced cost for premium features",
                "Increased engine acceleration",
                "Bonus for competition rewards",
                "Increased prestige facilities deliveries"};

            if (t > sizeof(desc) / sizeof(desc[0])) {
                return "Unknow effect";
            }

            return desc[t];
        }

        uint8_t t{};
    };

    struct information {
        int id;
        int effect;
        type ptype{0};
        int art_id;
        int goods_id[2]{};

        std::string str() const;
    };

    personality(information i, std::string name, std::string description, const std::string* icon1,
                const std::string* icon2);

    std::string str() const;
    std::string get_art_url() const;

    static const personality unknown;
    static const std::string unknown_icon;

    information info;   // reminder: keep member order equal to ctor init list for move
    std::string name;
    std::string description;
    const std::string* main_icon;
    const std::string* sec_icon;
};

struct auction {
    static constexpr std::chrono::seconds s_duration{3300};
    static constexpr std::chrono::seconds s_pause{300};
    static constexpr std::chrono::seconds s_request_wait{30};

    // Client adds 3 seconds of delay
    static constexpr std::chrono::seconds s_client_delay{3};

    // (delay * 2) Matchs client delay and adds 3s delay
    // the discord auction time will end 3s earlier to client, 6s to server
    static constexpr std::chrono::seconds s_discord_extra_delay{s_client_delay * 2};

    int personality_id;
    std::chrono::seconds end_time;
    std::string id;

    std::string str() const;
};

struct active_auction : public auction {
    active_auction(const auction& au, std::chrono::system_clock::time_point server_time, const personality* p)
        : auction(au), ends_at(server_time + au.end_time), p(p) {}

    bool has_ended() { return std::chrono::system_clock::now() > ends_at; }
    bool has_interval_timer(int interval) { return timers_.find(interval) != timers_.end(); }
    std::chrono::system_clock::time_point client_ends_at() { return ends_at - auction::s_discord_extra_delay; }

    std::chrono::system_clock::duration time_left() {
        return std::chrono::abs(ends_at - std::chrono::system_clock::now());
    }

    std::chrono::system_clock::duration wait_delay_for_interval(int interval) {
        return std::chrono::abs(time_left() - std::chrono::minutes{interval} - auction::s_discord_extra_delay);
    }

    bool expired_for_interval(int interval) {
        return time_left() < std::chrono::minutes{interval};
    }

    std::chrono::system_clock::time_point ends_at;
    const personality* p;
    std::unordered_map<int, dpp::timer> timers_;
};

inline bool operator==(const personality::type& lhs, const personality::type& rhs) { return lhs.t == rhs.t; }
inline bool operator==(const personality::type& lhs, const uint8_t rhs) { return lhs.t == rhs; }
inline bool operator==(const uint8_t lhs, const personality::type& rhs) { return lhs == rhs.t; }

void from_json(const nlohmann::json& j, auction& au);
void to_json(nlohmann::json& j, const auction& au);

void from_json(const nlohmann::json& j, personality::information& info);
void to_json(nlohmann::json& j, const personality::information& info);

}   // namespace railcord

#endif   // !PERSONALITY_H
