#include <fmt/format.h>

#include "commands.h"
#include "lucy.h"

namespace railcord::cmd {
using namespace std::chrono;

Ping::Ping(Lucy* lucy) : Base_Cmd("ping", "Ping pong!", seconds{3}, lucy) {}

dpp::slashcommand Ping::build() { return dpp::slashcommand(name_, description_, lucy_->bot.me.id); }

void Ping::handle_slash_interaction(const dpp::slashcommand_t& event) {
    dpp::message m{};
    m.set_content(fmt::format("<@&{}> pong", uint64_t(lucy_->alert_manager()->get_alert_role())));
    m.allowed_mentions.parse_roles = true;
    event.reply(m);
}

}   // namespace railcord::cmd
