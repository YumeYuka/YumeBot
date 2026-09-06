module event.pending;

import std;
import telegram.user;

auto PendingRepository::save(PendingJoinRequest request) -> void {
    by_user_id_[request.user_id] = std::move(request);
}

auto PendingRepository::find_by_user_id(TelegramId user_id) const -> std::optional<PendingJoinRequest> {
    if (const auto it = by_user_id_.find(user_id); it != by_user_id_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto PendingRepository::find_user_id_by_username(std::string_view username) const -> std::optional<TelegramId> {
    if (const auto it = username_to_user_id_.find(std::string{username}); it != username_to_user_id_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto PendingRepository::remove_by_user_id(TelegramId user_id) -> void {
    by_user_id_.erase(user_id);
}

auto PendingRepository::mark_approved(TelegramId user_id) -> void {
    approved_user_ids_.insert(user_id);
}

auto PendingRepository::consume_approved(TelegramId user_id) -> bool {
    if (const auto it = approved_user_ids_.find(user_id); it != approved_user_ids_.end()) {
        approved_user_ids_.erase(it);
        return true;
    }
    return false;
}

auto PendingRepository::record_user(const User &user) -> void {
    if (user.username.has_value() && !user.username->empty()) {
        username_to_user_id_[*user.username] = user.id;
    }
}

auto PendingRepository::remove_expired(std::int64_t now_millis) -> std::vector<PendingJoinRequest> {
    std::vector<PendingJoinRequest> expired;
    for (auto it = by_user_id_.begin(); it != by_user_id_.end();) {
        const auto &pending = it->second;
        if (pending.expires_at_millis.has_value()
            && *pending.expires_at_millis <= now_millis
            && !pending.test_only) {
            expired.push_back(pending);
            it = by_user_id_.erase(it);
        } else {
            ++it;
        }
    }
    return expired;
}
