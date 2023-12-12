#include <algorithm>
#include <vector>

#include <dpp/dpp.h>

#include "commands.h"
#include "logger.h"
#include "lucy.h"
#include "personality.h"

namespace railcord::cmd {

Command_handler::Command_handler() {}

Command_handler::~Command_handler() {
    for (auto&& c : cmds_) {
        delete c;
    }
}

void Command_handler::on_slash_cmd(Lucy* lucy) {
    lucy->bot.on_slashcommand([this, lucy](const dpp::slashcommand_t& event) {
        {
            auto whitelist = lucy->user_whitelist();
            auto user = std::find(whitelist.begin(), whitelist.end(), event.command.usr.id);
            if (user == whitelist.end()) {
                event.reply(dpp::message{"You have no permissions for this command!"}.set_flags(dpp::m_ephemeral));
                return;
            }
        }

        const std::string& command_name = event.command.get_command_name();

        auto cmd = std::find_if(cmds_.begin(), cmds_.end(), [&](auto& c) { return c->name() == command_name; });

        if (cmd == cmds_.end()) {
            logger->warn("Command {} not found", command_name);
            event.reply();
            return;
        }

        if ((*cmd)->is_on_cooldown()) {
            event.reply("Command is on cooldown");
            return;
        }

        (*cmd)->update_last_run_time();
        (*cmd)->handle_slash_interaction(event);
    });
}

void Command_handler::on_form_submit(Lucy* lucy) {
    lucy->bot.on_form_submit([this](const dpp::form_submit_t& event) {
        for (const auto& [prefix, cmd] : cmd_prefix_handlers_) {
            if (event.custom_id.rfind(prefix, 0) == 0) {
                cmd->handle_form_submit(event);
                return;
            }
        }

        logger->warn("on_form_submit id \"{}\" does not have a handler", event.custom_id);
        event.reply();
    });
}

void Command_handler::on_button_click(Lucy* lucy) {
    lucy->bot.on_button_click([this](const dpp::button_click_t& event) {
        for (const auto& [prefix, cmd] : cmd_prefix_handlers_) {
            if (event.custom_id.rfind(prefix, 0) == 0) {
                cmd->handle_button_click(event);
                return;
            }
        }

        logger->warn("on_button_click id \"{}\" does not have a handler", event.custom_id);
        event.reply();
    });
}

void Command_handler::on_select_click(Lucy* lucy) {
    lucy->bot.on_select_click([this](const dpp::select_click_t& event) {
        for (const auto& [prefix, cmd] : cmd_prefix_handlers_) {
            if (event.custom_id.rfind(prefix, 0) == 0) {
                cmd->handle_select_click(event);
                return;
            }
        }

        logger->warn("on_select_click id \"{}\" does not have a handler", event.custom_id);
        event.reply();
    });
}

void Command_handler::add_command(Base_Cmd* cmd) {
    if (std::find(cmds_.begin(), cmds_.end(), cmd) == cmds_.end()) {
        cmds_.push_back(cmd);

        const auto& cmd_prefix = cmd->handler_prefix();
        if (cmd_prefix) {
            logger->debug("Adding prefix handler {}:{}", *cmd_prefix, cmd->name());
            cmd_prefix_handlers_.emplace_back(*cmd_prefix, cmd);
        }
    }
}

void Command_handler::register_commands(Lucy* lucy) {
    lucy->bot.on_ready([this, lucy](const dpp::ready_t&) {
        if (dpp::run_once<struct register_bot_commands>()) {
            std::vector<dpp::slashcommand> commands;
            for (const auto& c : cmds_) {
                logger->info("Registering command: {}", c->name());
                commands.emplace_back(c->build());
            }

            lucy->bot.guild_bulk_command_create(commands, test_server);
        }
    });
}

}   // namespace railcord::cmd
