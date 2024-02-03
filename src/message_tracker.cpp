#include <algorithm>

#include "logger.h"
#include "message_tracker.h"

namespace railcord {

MessageTracker::MessageTracker(dpp::cluster* bot) : bot_(bot) {}

MessageTracker::~MessageTracker() { delete_all_messages(true); }

void MessageTracker::add_message(const SentMessage& msg) {
    std::lock_guard<std::mutex> lock{mtx_};
    msgs_.push_back(msg);
}

void MessageTracker::add_message(const dpp::snowflake id, const dpp::snowflake channel_id) {
    std::lock_guard<std::mutex> lock{mtx_};
    msgs_.emplace_back(id, channel_id);
}

void MessageTracker::remove_message(const dpp::snowflake id) {
    std::lock_guard<std::mutex> lock{mtx_};
    msgs_.erase(
        std::remove_if(msgs_.begin(), msgs_.end(), [&](const auto& stored_msg) { return stored_msg.id == id; }));
}

void MessageTracker::delete_message(const dpp::snowflake id, const std::string& info) {
    std::unique_lock<std::mutex> lock{mtx_};
    auto it = std::find_if(msgs_.begin(), msgs_.end(), [&](const auto& stored_msg) { return stored_msg.id == id; });
    if (it != msgs_.end()) {
        logger->info("Deleting message{} id={}", info, static_cast<uint64_t>(it->id));
        bot_->message_delete(it->id, it->channel_id);
        lock.unlock();
        remove_message(id);
    }
}

void MessageTracker::delete_all_messages(bool wait_deletion) {
    std::lock_guard<std::mutex> lock{mtx_};

    if (msgs_.empty()) {
        return;
    }

    std::function<void(const SentMessage&)> delete_msg;
    if (wait_deletion) {
        logger->info("Waiting for message deletion");
        delete_msg = [&](const SentMessage& msg) { bot_->message_delete_sync(msg.id, msg.channel_id); };
    } else {
        delete_msg = [&](const SentMessage& msg) { bot_->message_delete(msg.id, msg.channel_id); };
    }

    for (const auto& msg : msgs_) {
        logger->info("Deleting message(all) id={}", static_cast<uint64_t>(msg.id));
        try {
            delete_msg(msg);
        } catch (const dpp::rest_exception& e) {
            logger->warn("Deleting message(all) id={} failed with: {}", static_cast<uint64_t>(msg.id), e.what());
        }
    }
    msgs_.clear();
}

}   // namespace railcord
