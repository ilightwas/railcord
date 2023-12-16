#ifndef SENT_MESSAGES_H
#define SENT_MESSAGES_H

#include <mutex>
#include <vector>

#include <dpp/dpp.h>

namespace railcord {

struct Sent_Message {
    Sent_Message(dpp::snowflake id, dpp::snowflake channel_id) : id(id), channel_id(channel_id) {}
    dpp::snowflake id;
    dpp::snowflake channel_id;
};

class Sent_Messages {
  public:
    Sent_Messages(dpp::cluster* bot);
    ~Sent_Messages();

    void add_message(const Sent_Message& msg);
    void add_message(const dpp::snowflake id, const dpp::snowflake channel_id);
    void remove_message(const dpp::snowflake id);
    void delete_message(const dpp::snowflake id, const std::string& info = "");
    void delete_all_messages(bool wait_deletion = false);

  private:
    dpp::cluster* bot_;
    std::vector<Sent_Message> msgs_;
    std::mutex mtx_;
};

}   // namespace railcord

#endif   // !SENT_MESSAGES_H
