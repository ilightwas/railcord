#include <dpp/dpp.h>

#include "cmdline_helper.h"
#include "lucy.h"
#include "util.h"

using namespace std::string_view_literals;

namespace railcord::cmd {

const std::unordered_map<std::string_view, BotAction> cmdline_actions_args = {
    {"--reg"sv, BotAction::REG_CMDS}, {"--clear"sv, BotAction::CLEAR_CMDS}, {"--leave"sv, BotAction::LEAVE_SRV}};

BotAction parse_cmdline(int argc, const char* argv[]) {
    if (argc <= 1)
        return BotAction::INIT;

    const auto found = cmdline_actions_args.find(argv[1]);
    if (found == cmdline_actions_args.end())
        return BotAction::INIT;

    return found->second;
}

static void leave_server(Lucy* lucy) {
    lucy->bot.on_ready([lucy](const dpp::ready_t&) {
        if (dpp::run_once<struct leave_server>()) {
            lucy->bot.current_user_get_guilds([lucy](const dpp::confirmation_callback_t& c) {
                const auto& guilds = c.get<dpp::guild_map>();
                const auto found = guilds.find(test_server);
                if (found == guilds.end()) {
                    logger->warn("Bot is not in server {}", test_server);
                    lucy->shutdown();
                    return;
                }
                logger->info("Leaving server {}", found->second.name);
                lucy->bot.current_user_leave_guild(test_server, [lucy](const dpp::confirmation_callback_t& c) {
                    dpp::utility::log_error()(c);
                    lucy->shutdown();
                });
            });
        }
    });
}

void do_cmdline_action(BotAction action, Lucy* lucy) {
    switch (action) {
        case BotAction::REG_CMDS: {
            lucy->cmd_handler()->load_all_commands();
            lucy->cmd_handler()->register_commands();
            break;
        }

        case BotAction::CLEAR_CMDS:
            lucy->cmd_handler()->deregister_commands();
            break;

        case BotAction::LEAVE_SRV:
            leave_server(lucy);
            break;
    }
}

}   // namespace railcord::cmd