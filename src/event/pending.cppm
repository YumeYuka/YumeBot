export module event.pending;

import std;
import telegram.types;
import telegram.user;

export struct PendingJoinRequest {
    TelegramId chat_id{};
    TelegramId user_id{};
    TelegramId user_chat_id{};
    TelegramId prompt_chat_id{};
    std::optional<std::int64_t> prompt_message_id{};
    std::optional<TelegramId> guide_chat_id{};
    std::optional<std::int64_t> guide_message_id{};
    std::optional<std::int64_t> expires_at_millis{};
    bool needs_unmute_on_success{false};
    bool test_only{false};
};

export class PendingRepository {
public:
    auto save(PendingJoinRequest request) -> void;

    [[nodiscard]] auto find_by_user_id(TelegramId user_id) const -> std::optional<PendingJoinRequest>;

    [[nodiscard]] auto find_user_id_by_username(std::string_view username) const -> std::optional<TelegramId>;

    auto remove_by_user_id(TelegramId user_id) -> void;

    auto mark_approved(TelegramId user_id) -> void;

    [[nodiscard]] auto consume_approved(TelegramId user_id) -> bool;

    auto record_user(const User &user) -> void;

    [[nodiscard]] auto remove_expired(std::int64_t now_millis) -> std::vector<PendingJoinRequest>;

private:
    std::unordered_map<TelegramId, PendingJoinRequest> by_user_id_;
    std::unordered_map<std::string, TelegramId> username_to_user_id_;
    std::unordered_set<TelegramId> approved_user_ids_;
};
