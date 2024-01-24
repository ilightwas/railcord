#include "commands.h"
#include "logger.h"
#include "lucy.h"

namespace railcord::cmd {
using namespace std::chrono;

Shutdown::Shutdown(Lucy* lucy) : Base_Cmd("shutdown", "Shuts down Lucy", seconds{10}, lucy) {}

dpp::slashcommand Shutdown::build() { return dpp::slashcommand(name_, description_, lucy_->bot.me.id); }

void Shutdown::handle_slash_interaction(const dpp::slashcommand_t& event) {
    if (event.command.get_issuing_user().id != Lucy::s_bot_owner) {
        event.reply(dpp::message{"You have no permissions for this command!"}.set_flags(dpp::m_ephemeral));
        return;
    }
    if (lucy_->is_running()) {
        logger->info("Received shutdown command, stopping..");
        event.reply(dpp::message{"Shutting down.. bye bye"});
        lucy_->shutdown();
    } else {
        logger->warn("Shutdown command received again..");
        event.reply(dpp::message{"Shutdown already ongoing!"}.set_flags(dpp::m_ephemeral));
    }
}

}   // namespace railcord::cmd
