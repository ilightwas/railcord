#include <algorithm>

#include <cpr/cpr.h>
#include <dpp/nlohmann/json.hpp>

#include "license.h"
#include "logger.h"
#include "lucyapi.h"
#include "util.h"

namespace railcord {

using namespace std::chrono;
using json = nlohmann::json;

void from_json(const nlohmann::json& j, License& l) {
    j.at("AuctionId").get_to(l.id);

    int s = j.at("StartTime");
    if (s < 0) {
        s = 0;
    }
    l.start = std::chrono::seconds{s};

    l.end = std::chrono::seconds{j.at("EndTime")};
    l.count = j.at("LicenceCount");
    l.min_price = j.at("MinimumPrice");
    l.good_type = j.at("ResourceType");
}

void to_json(nlohmann::json&, const License&) {}

static std::deque<License> request_licenses() {
    cpr::Response r = cpr::Get(cpr::Url{api_endpoints.at(api::license_id)}, cpr::Timeout{seconds{s_request_license_timeout}});
    if (r.status_code != 200) {
        if (r.status_code == 0) {
            logger->warn("License request timed out");
        } else {
            logger->warn("License request failed with status code={}", r.status_code);
        }
        return {};
    }

    try {
        return nlohmann::json::parse(r.text).at("Body").get<std::deque<License>>();
    } catch (const json::exception& e) {
        logger->warn("Parsing json failed with: {}", e.what());
        return {};
    } catch (const std::exception& e) {
        logger->warn("request_licenses failed with: {}", e.what());
        return {};
    }
}

void License_Manager::fetch_new_licenses() {
    auto request_start = std::chrono::steady_clock::now();
    auto l = request_licenses();

    if (l.empty()) {
        logger->warn("Fetch new licenses failed");
        return;
    }

    t_offset_ = std::chrono::system_clock::now();
    logger->info("Time taken by license request {:.2f}s",
                 duration_cast<duration<float>>(std::chrono::abs(steady_clock::now() - request_start)).count());

    l.erase(std::remove_if(l.begin(), l.end(), [&](const License& ll) { return ll.start.count() == 0; }), l.end());

    std::sort(l.begin(), l.end(), [](const License& a, const License& b) { return a.start < b.start; });

    licenses_ = std::move(l);
}

void License_Manager::update_state() {
    std::lock_guard<std::mutex> lock{mtx_};
    if (licenses_.size() < 4 * 12) {   // 12hrs
        logger->info("Less than 12hrs ahead of licenses, fetching new licenses");
        fetch_new_licenses();
        return;
    }

    licenses_.erase(
        std::remove_if(licenses_.begin(), licenses_.end(), [&](const License& license) { return expired(license); }),
        licenses_.end());
}

std::chrono::system_clock::time_point License_Manager::get_start_tp(const License& license) {
    return t_offset_ + license.start;
}

std::chrono::system_clock::time_point License_Manager::get_end_tp(const License& license) {
    return t_offset_ + license.end;
}

std::chrono::system_clock::duration License_Manager::time_left_to_start(const License& license) {
    return std::chrono::abs(get_start_tp(license) - system_clock::now());
}

bool License_Manager::expired(const License& license) { return system_clock::now() > get_end_tp(license); }

bool License_Manager::is_currently_active(const License& license) {
    auto now = system_clock::now();
    return now > get_start_tp(license) && now < get_end_tp(license);
}

std::optional<License> License_Manager::get_next_license(int good_type) {
    std::lock_guard<std::mutex> lock{mtx_};
    while (true) {
        auto found = std::find_if(licenses_.begin(), licenses_.end(), [&](const License& l) {
            return l.good_type == good_type;
        });
        if (found != licenses_.end()) {
            if (expired(*found)) {
                licenses_.erase(found);
            } else {
                return *found;
            }
        } else {
            return {};
        }
    }
}

}   // namespace railcord
