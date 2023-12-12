#include "commands.h"
#include "lucy.h"

namespace railcord::cmd {
using namespace std::chrono;

Watch::Watch(Lucy* lucy) : Base_Cmd("watch", "Starts watching workers", seconds{10}, lucy) {}

dpp::slashcommand Watch::build() { return dpp::slashcommand(name_, description_, lucy_->bot.me.id); }

void Watch::handle_slash_interaction(const dpp::slashcommand_t& event) {
    if (lucy_->watcher()->is_watching()) {
        event.reply("Already watching!");
    } else {
        event.reply("Starting workers watch..");
        lucy_->watcher()->run();
    }
}

}   // namespace railcord::cmd
