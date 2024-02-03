#ifndef SENT_MESSAGES_H
#define SENT_MESSAGES_H

#include <mutex>
#include <string>
#include <vector>

#include <dpp/dpp.h>

namespace railcord {

struct SentMessage {
    SentMessage(dpp::snowflake id, dpp::snowflake channel_id) : id(id), channel_id(channel_id) {}
    dpp::snowflake id;
    dpp::snowflake channel_id;
};

class MessageTracker {
  public:
    MessageTracker(dpp::cluster* bot);
    ~MessageTracker();

    void add_message(const SentMessage& msg);
    void add_message(const dpp::snowflake id, const dpp::snowflake channel_id);
    void remove_message(const dpp::snowflake id);
    void delete_message(const dpp::snowflake id, const std::string& info = "");
    void delete_all_messages(bool wait_deletion = false);

    const static uint64_t s_delete_message_delay = 180;

  private:
    dpp::cluster* bot_;
    std::vector<SentMessage> msgs_;
    std::mutex mtx_;
};

}   // namespace railcord

#endif   // !SENT_MESSAGES_H
