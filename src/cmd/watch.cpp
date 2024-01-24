#include "commands.h"
#include "lucy.h"

namespace railcord::cmd {
using namespace std::chrono;

Watch::Watch(Lucy* lucy) : Base_Cmd("watch", "Starts watching workers", seconds{10}, lucy) {}

inline constexpr const char* s_horizon_cmd_option{"active_only_horizon_msg"};

dpp::slashcommand Watch::build() {
    return dpp::slashcommand(name_, description_, lucy_->bot.me.id)
        .add_option(dpp::command_option(dpp::co_boolean, s_horizon_cmd_option,
                                        "Only sends horizon messages for active alerts", true)
                        .add_choice(dpp::command_option_choice("Enabled", true))
                        .add_choice(dpp::command_option_choice("Disabled", false)));
}

void Watch::handle_slash_interaction(const dpp::slashcommand_t& event) {
    if (lucy_->watcher()->is_watching()) {
        event.reply(dpp::message{"Already watching!"}.set_flags(dpp::m_ephemeral));
    } else {
        event.reply(dpp::message{"Starting workers watch.."}.set_flags(dpp::m_ephemeral));
        personality_watcher* watcher = lucy_->watcher();
        watcher->set_active_only_horizon_msg(std::get<bool>(event.get_parameter(s_horizon_cmd_option)));
        watcher->run();
    }
}

}   // namespace railcord::cmd
