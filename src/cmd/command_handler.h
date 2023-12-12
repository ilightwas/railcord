#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include <utility>
#include <vector>

namespace railcord {
class Lucy;
}

namespace railcord::cmd {

class Base_Cmd;

class Command_handler {
  public:
    Command_handler();
    Command_handler(const Command_handler&) = delete;
    Command_handler& operator=(const Command_handler&) = delete;
    Command_handler& operator=(Command_handler&&) = delete;
    ~Command_handler();

    void on_slash_cmd(Lucy* lucy);
    void on_form_submit(Lucy* lucy);
    void on_button_click(Lucy* lucy);
    void on_select_click(Lucy* lucy);

    void add_command(Base_Cmd* cmd);
    void register_commands(Lucy* lucy);

  private:
    std::vector<Base_Cmd*> cmds_;
    std::vector<std::pair<std::string, Base_Cmd*>> cmd_prefix_handlers_;   // prefix -> cmd
};

}   // namespace railcord::cmd

#endif   // !COMMAND_HANDLER_H
