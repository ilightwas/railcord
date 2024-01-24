#include "commands.h"
#include "lucy.h"

namespace railcord::cmd {
using namespace std::chrono;

Stop_watch::Stop_watch(Lucy* lucy) : Base_Cmd("stop_watch", "Stops watching workers", seconds{10}, lucy) {}

dpp::slashcommand Stop_watch::build() { return dpp::slashcommand(name_, description_, lucy_->bot.me.id); }

void Stop_watch::handle_slash_interaction(const dpp::slashcommand_t& event) {
    if (lucy_->watcher()->is_watching()) {
        event.thinking(true);
        lucy_->watcher()->stop();
        event.edit_original_response(dpp::message{"Workers watch stopped!"});
    } else {
        event.reply("Workers watch is not running!");
    }
}

}   // namespace railcord::cmd
