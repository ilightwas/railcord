#ifndef PERSONALITY_WATCHER_H
#define PERSONALITY_WATCHER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include <dpp/dpp.h>

#include "message_tracker.h"
#include "personality.h"

namespace railcord {

class GameData;
class Alert_Info;
class Alert_Manager;

class personality_watcher {
  public:
    personality_watcher(dpp::cluster* bot, GameData* g, Alert_Manager* al_m);
    personality_watcher() = delete;
    personality_watcher(const personality_watcher&) = delete;
    personality_watcher(personality_watcher&&) = delete;
    personality_watcher& operator=(const personality_watcher&) = delete;
    personality_watcher& operator=(personality_watcher&&) = delete;
    ~personality_watcher();

    void run();
    void stop();
    bool is_watching();
    void set_active_only_horizon_msg(bool enabled) { active_only_horizon_msg_ = enabled; }
    bool active_only_horizon_msg() { return active_only_horizon_msg_; }

    bool is_using_local_time() { return use_local_time_; }
    void set_using_local_time(bool use_local_time) { use_local_time_ = use_local_time; }

    static constexpr int s_request_auction_timeout = 90;   // seconds
    static constexpr int s_sync_time_timeout = 90;         // seconds
    static constexpr int s_max_tries = 5;

  private:
    void personality_update();
    std::optional<std::vector<auction>> request_auctions();
    void process_auctions(std::vector<auction>& auctions);

    bool sync_time();
    void do_sync_time(std::chrono::system_clock::time_point server_time,
                      std::chrono::steady_clock::time_point request_time);
    void add_wait_time(active_auction* au);
    void wait();
    std::chrono::system_clock::time_point server_time_now();

    void send_discord_msg(const dpp::message& msg, std::chrono::system_clock::duration wait_delete);
    void reset();

    dpp::cluster* bot_;
    GameData* gamedata;
    Alert_Manager* alert_manager_;

    std::atomic_bool watching_;
    std::thread personality_thread_;
    std::condition_variable cv_;
    std::mutex mtx_;

    bool use_local_time_;
    bool active_only_horizon_msg_;
    std::chrono::milliseconds server_delta_;
    std::chrono::steady_clock::time_point at_sync_time_;
    std::chrono::system_clock::time_point server_time_;

    std::deque<std::chrono::system_clock::duration> wait_times_;
    MessageTracker sent_msgs_;
};
}   // namespace railcord

#endif   // !PERSONALITY_WATCHER_H
