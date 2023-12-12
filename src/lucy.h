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

    void init(bool reg_cmds);
    void load_settings();

    bool is_running() { return running_.load(); }
    void shutdown();

    Gamedata* gamedata() { return &gamedata_; }
    Alert_Manager* alert_manager() { return &alert_manager_; }
    personality_watcher* watcher() { return &watcher_; }
    const std::vector<dpp::snowflake>& user_whitelist() { return whitelist_; }
    const std::vector<dpp::emoji>& custom_emojis() { return custom_emojis_; }

    static dpp::snowflake s_bot_owner;

    dpp::cluster bot;

  private:
    void bot_on_ready(bool reg_cmds);
    void bot_on_slash_cmd();
    void bot_on_form_submit();
    void bot_on_button_click();
    void bot_on_select_click();
    void load_commands();

    std::atomic_bool running_;
    Gamedata gamedata_;
    Alert_Manager alert_manager_;
    personality_watcher watcher_;
    cmd::Command_handler cmd_handler;
    std::vector<dpp::snowflake> whitelist_;
    std::vector<dpp::emoji> custom_emojis_;
};

}   // namespace railcord

#endif   // !LUCY_H
