#include "commands.h"
#include "lucy.h"

namespace railcord::cmd {
using namespace std::chrono;

Save_Settings::Save_Settings(Lucy* lucy) : Base_Cmd("save_settings", "Save current settings", seconds{5}, lucy) {}

dpp::slashcommand Save_Settings::build() { return dpp::slashcommand(name_, description_, lucy_->bot.me.id); }

void Save_Settings::handle_slash_interaction(const dpp::slashcommand_t& event) {
    if (lucy_->alert_manager()->save_state()) {
        event.reply(dpp::message{"Settings saved"}.set_flags(dpp::m_ephemeral));
    } else {
        event.reply(dpp::message{"!! Something went wrong saving settings"}.set_flags(dpp::m_ephemeral));
    }
}

}   // namespace railcord::cmd
