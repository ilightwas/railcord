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
    Command_handler(Lucy* lucy);
    Command_handler(const Command_handler&) = delete;
    Command_handler& operator=(const Command_handler&) = delete;
    Command_handler& operator=(Command_handler&&) = delete;
    ~Command_handler();

    void on_slash_cmd();
    void on_form_submit();
    void on_button_click();
    void on_select_click();

    void add_command(Base_Cmd* cmd);
    void load_all_commands();
    void register_commands();
    void deregister_commands();

  private:
    Lucy* lucy_;
    std::vector<Base_Cmd*> cmds_;
    std::vector<std::pair<std::string, Base_Cmd*>> cmd_prefix_handlers_;   // prefix -> cmd
};

}   // namespace railcord::cmd

#endif   // !COMMAND_HANDLER_H
