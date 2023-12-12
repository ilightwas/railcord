#include <fmt/format.h>

#include "commands.h"
#include "lucy.h"

namespace railcord::cmd {
using namespace std::chrono;

Set_channel::Set_channel(Lucy* lucy) : Base_Cmd("set_channel", "Sets the channel for workers", seconds{5}, lucy) {}

dpp::slashcommand Set_channel::build() { return dpp::slashcommand(name_, description_, lucy_->bot.me.id); }

void Set_channel::handle_slash_interaction(const dpp::slashcommand_t& event) {
    lucy_->alert_manager()->set_alert_channel(event.command.channel.id);
    event.reply(fmt::format("Watch channel set to: {}", event.command.channel.name));
}

}   // namespace railcord::cmd
