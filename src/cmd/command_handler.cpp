#include <algorithm>
#include <mutex>
#include <vector>

#include <dpp/dpp.h>

#include "command_handler.h"
#include "commands.h"
#include "logger.h"
#include "lucy.h"
#include "personality.h"

namespace railcord::cmd {

Command_handler::Command_handler(Lucy* lucy) : lucy_(lucy) {}

Command_handler::~Command_handler() {
    for (auto&& c : cmds_) {
        delete c;
    }
}

static bool can_use_command(const dpp::slashcommand_t& event, Lucy* lucy) {
    if (event.command.get_command_name() == "license") {   // temporary
        return true;
    }

    const auto& whitelist = lucy->user_whitelist();
    const auto& member = event.command.member;

    auto found = std::find(whitelist.cbegin(), whitelist.cend(), member.user_id);
    if (found != whitelist.end()) {
        return true;
    }

    found = std::find(member.get_roles().cbegin(), member.get_roles().cend(), lucy->bot_admin_role());
    if (found != member.get_roles().end()) {
        return true;
    }

    return false;
}

void Command_handler::on_slash_cmd() {
    lucy_->bot.on_slashcommand([this](const dpp::slashcommand_t& event) {
        if (!can_use_command(event, lucy_)) {
            event.reply(dpp::message{"You have no permissions for this command!"}.set_flags(dpp::m_ephemeral));
            return;
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

void Command_handler::on_form_submit() {
    lucy_->bot.on_form_submit([this](const dpp::form_submit_t& event) {
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

void Command_handler::on_button_click() {
    lucy_->bot.on_button_click([this](const dpp::button_click_t& event) {
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

void Command_handler::on_select_click() {
    lucy_->bot.on_select_click([this](const dpp::select_click_t& event) {
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

void Command_handler::load_all_commands() {
    static std::once_flag s_flag;
    std::call_once(s_flag, [this]() {
        add_command(new cmd::Ping(lucy_));
        add_command(new cmd::Watch(lucy_));
        add_command(new cmd::Stop_watch(lucy_));
        add_command(new cmd::Shutdown(lucy_));
        add_command(new cmd::Set_channel(lucy_));
        add_command(new cmd::Alert_on(lucy_));
        add_command(new cmd::Save_Settings(lucy_));
        add_command(new cmd::Remove_Custom_Message(lucy_));
        add_command(new cmd::License_Bid(lucy_));
    });
}

void Command_handler::register_commands() {
    lucy_->bot.on_ready([this](const dpp::ready_t&) {
        if (dpp::run_once<struct register_bot_commands>()) {
            std::vector<dpp::slashcommand> commands;
            for (const auto& c : cmds_) {
                logger->info("Registering command: {}", c->name());
                commands.emplace_back(c->build());
            }

            lucy_->bot.guild_bulk_command_create(
                commands, test_server, [lucy = this->lucy_](const dpp::confirmation_callback_t& c) {
                    dpp::utility::log_error()(c);
                    lucy->shutdown();
                });
        }
    });
}

void Command_handler::deregister_commands() {
    lucy_->bot.on_ready([lucy = this->lucy_](const dpp::ready_t&) {
        if (dpp::run_once<struct deregister_bot_commands>()) {
            logger->info("Deregistering all commands from server {}", test_server);
            lucy->bot.guild_bulk_command_create({}, test_server, [lucy](const dpp::confirmation_callback_t& c) {
                dpp::utility::log_error()(c);
                lucy->shutdown();
            });
        }
    });
}
}   // namespace railcord::cmd
