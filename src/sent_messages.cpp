#include <algorithm>

#include "logger.h"
#include "sent_messages.h"

namespace railcord {

sent_messages::sent_messages(dpp::cluster* bot) : bot_(bot) {}

sent_messages::~sent_messages() { delete_all_messages(true); }

void sent_messages::add_message(const sent_message& msg) {
    std::lock_guard<std::mutex> lock{mtx_};
    msgs_.push_back(msg);
}

void sent_messages::add_message(const dpp::snowflake id, const dpp::snowflake channel_id) {
    std::lock_guard<std::mutex> lock{mtx_};
    msgs_.emplace_back(id, channel_id);
}

void sent_messages::remove_message(const dpp::snowflake id) {
    std::lock_guard<std::mutex> lock{mtx_};
    msgs_.erase(
        std::remove_if(msgs_.begin(), msgs_.end(), [&](const auto& stored_msg) { return stored_msg.id == id; }));
}

void sent_messages::delete_message(const dpp::snowflake id, const std::string& info) {
    std::unique_lock<std::mutex> lock{mtx_};
    auto it = std::find_if(msgs_.begin(), msgs_.end(), [&](const auto& stored_msg) { return stored_msg.id == id; });
    if (it != msgs_.end()) {
        logger->info("Deleting message{} id={}", info, static_cast<uint64_t>(it->id));
        bot_->message_delete(it->id, it->channel_id);
        lock.unlock();
        remove_message(id);
    }
}

void sent_messages::delete_all_messages(bool wait_deletion) {
    std::lock_guard<std::mutex> lock{mtx_};
    for (const auto& msg : msgs_) {
        logger->info("Deleting message(all) id={}", static_cast<uint64_t>(msg.id));
        if (wait_deletion) {
            try {
                bot_->message_delete_sync(msg.id, msg.channel_id);
            } catch (const dpp::rest_exception& e) {
                logger->warn("Deleting message(all) id={} failed with: {}", static_cast<uint64_t>(msg.id), e.what());
            }
        } else {
            bot_->message_delete(msg.id, msg.channel_id);
        }
    }
    msgs_.clear();
}

}   // namespace railcord
