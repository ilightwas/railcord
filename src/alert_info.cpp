#include <algorithm>

#include "alert_info.h"

namespace railcord {

const std::vector<int> Alert_Info::s_intervals{1, 5, 10, 20, 30, 60};

Alert_Info::Alert_Info(personality::type t) : type_(t) {}

void Alert_Info::enable_interval(int minutes) {
    const auto it = std::find(enabled_intervals_.begin(), enabled_intervals_.end(), minutes);
    if (it == enabled_intervals_.end()) {
        enabled_intervals_.push_back(minutes);
    }
}

void Alert_Info::disable_interval(int minutes) {
    const auto it = std::find(enabled_intervals_.begin(), enabled_intervals_.end(), minutes);
    if (it != enabled_intervals_.end()) {
        enabled_intervals_.erase(it);
    }
}

bool Alert_Info::interval_is_enabled(int minutes) {
    const auto it = std::find(enabled_intervals_.begin(), enabled_intervals_.end(), minutes);
    return it != enabled_intervals_.end();
}

}   // namespace railcord
