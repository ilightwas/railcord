#ifndef LICENSE_H
#define LICENSE_H

#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <string>

#include <dpp/dpp.h>
#include <dpp/nlohmann/json.hpp>

namespace railcord {

struct GameResource;
using Good = GameResource;

static constexpr int s_request_license_timeout = 90;   // seconds

struct License {
    std::string id;
    std::chrono::seconds start;
    std::chrono::seconds end;
    int count;
    int min_price;
    int good_type;

    struct Embed_Data {
        const std::vector<dpp::snowflake> users;
        const Good* good;
        const License* license;
        const std::chrono::system_clock::time_point end_tp;
    };
};

class License_Manager {
  public:
    License_Manager() = default;
    License_Manager(const License_Manager&) = delete;
    License_Manager(License_Manager&&) = delete;
    License_Manager& operator=(const License_Manager&) = delete;
    License_Manager& operator=(License_Manager&&) = delete;

    std::optional<License> get_next_license(int good_type);
    void update_state();
    std::chrono::system_clock::time_point get_start_tp(const License& license);
    std::chrono::system_clock::time_point get_end_tp(const License& license);
    std::chrono::system_clock::duration time_left_to_start(const License& license);
    bool expired(const License& license);
    bool is_currently_active(const License& license);

  private:
    void fetch_new_licenses();

    std::deque<License> licenses_;
    std::chrono::system_clock::time_point t_offset_;
    mutable std::mutex mtx_;
};

void from_json(const nlohmann::json& j, License& l);
void to_json(nlohmann::json& j, const License& l);

}   // namespace railcord

#endif   // !LICENSE_H
