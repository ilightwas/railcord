#ifndef CMDLINE_HELPER_H
#define CMDLINE_HELPER_H

#include <string_view>
#include <unordered_map>

namespace railcord {
class Lucy;
}

namespace railcord::cmd {

enum class BotAction { INIT, REG_CMDS, CLEAR_CMDS, LEAVE_SRV };
extern const std::unordered_map<std::string_view, BotAction> cmdline_actions_args;

BotAction parse_cmdline(int argc, const char* argv[]);
void do_cmdline_action(BotAction action, Lucy* lucy);

}   // namespace railcord::cmd

#endif   // !CMDLINE_HELPER_H
