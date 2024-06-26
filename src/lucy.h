#ifndef LUCY_H
#define LUCY_H

#include <atomic>
#include <string>
#include <vector>

#include <dpp/dpp.h>

#include "alert_manager.h"
#include "cmd/command_handler.h"
#include "gamedata.h"
#include "personality_watcher.h"

namespace railcord {

extern dpp::snowflake test_server;
inline constexpr const char* token_file = "resources/token.txt";

class Lucy {
  public:
    Lucy();
    Lucy(const std::string& token);
    Lucy(const Lucy&) = delete;
    Lucy(Lucy&&) = delete;
    Lucy& operator=(const Lucy&) = delete;
    Lucy& operator=(Lucy&&) = delete;

    void init(int argc, const char* argv[]);
    void load_settings();

    bool is_running() { return running_.load(); }
    void shutdown();

    GameData* gamedata() { return &gamedata_; }
    Alert_Manager* alert_manager() { return &alert_manager_; }
    personality_watcher* watcher() { return &watcher_; }
    cmd::Command_handler* cmd_handler() { return &cmd_handler_; }
    const std::vector<dpp::snowflake>& user_whitelist() { return whitelist_; }
    const std::vector<dpp::emoji>& custom_emojis() { return custom_emojis_; }
    dpp::snowflake bot_admin_role() const { return bot_admin_role_; }
    static dpp::snowflake s_bot_owner;

    dpp::cluster bot;

  private:
    std::atomic_bool running_;
    GameData gamedata_;
    Alert_Manager alert_manager_;
    personality_watcher watcher_;
    cmd::Command_handler cmd_handler_;
    dpp::snowflake bot_admin_role_;
    std::vector<dpp::snowflake> whitelist_;
    std::vector<dpp::emoji> custom_emojis_;
};

}   // namespace railcord

#endif   // !LUCY_H
