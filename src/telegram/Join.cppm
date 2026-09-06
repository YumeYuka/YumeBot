export module telegram.join;

import std;
import telegram.chat;
import telegram.json;
import telegram.types;
import telegram.user;

export struct ChatInviteLink {
    std::string invite_link{};
    User creator{};
    bool creates_join_request{false};
    bool is_primary{false};
    bool is_revoked{false};
    std::optional<std::string> name{};
    std::optional<UnixTime> expire_date{};
    std::optional<int> member_limit{};
    std::optional<int> pending_join_request_count{};

    static auto from_json(const JsonValue &json) -> ChatInviteLink {
        ChatInviteLink link;
        link.invite_link = json_string(json, "invite_link").value_or("");
        if (const auto *creator = json_object(json, "creator")) {
            link.creator = User::from_json(*creator);
        }
        link.creates_join_request = json_bool(json, "creates_join_request").value_or(false);
        link.is_primary = json_bool(json, "is_primary").value_or(false);
        link.is_revoked = json_bool(json, "is_revoked").value_or(false);
        link.name = json_string(json, "name");
        link.expire_date = json_i64(json, "expire_date");
        if (const auto limit = json_i64(json, "member_limit")) {
            link.member_limit = static_cast<int>(*limit);
        }
        if (const auto pending = json_i64(json, "pending_join_request_count")) {
            link.pending_join_request_count = static_cast<int>(*pending);
        }
        return link;
    }
};

export struct ChatJoinRequest {
    Chat chat{};
    User from{};
    TelegramId user_chat_id{};
    UnixTime date{};
    std::optional<std::string> bio{};
    std::optional<ChatInviteLink> invite_link{};

    static auto from_json(const JsonValue &json) -> ChatJoinRequest {
        ChatJoinRequest request;
        if (const auto *chat = json_object(json, "chat")) {
            request.chat = Chat::from_json(*chat);
        }
        if (const auto *from = json_object(json, "from")) {
            request.from = User::from_json(*from);
        }
        request.user_chat_id = json_i64(json, "user_chat_id").value_or(0);
        request.date = json_i64(json, "date").value_or(0);
        request.bio = json_string(json, "bio");
        if (const auto *link = json_object(json, "invite_link")) {
            request.invite_link = ChatInviteLink::from_json(*link);
        }
        return request;
    }
};

export struct CreateChatInviteLinkRequest {
    TelegramId chat_id{};
    std::optional<std::string> name{};
    std::optional<UnixTime> expire_date{};
    bool creates_join_request{true};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "chat_id", chat_id);
        json_put(object, "name", name);
        json_put(object, "expire_date", expire_date);
        json_put(object, "creates_join_request", creates_join_request);
        return JsonValue::object(std::move(object));
    }
};

export struct ApproveChatJoinRequest {
    TelegramId chat_id{};
    TelegramId user_id{};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "chat_id", chat_id);
        json_put(object, "user_id", user_id);
        return JsonValue::object(std::move(object));
    }
};

export struct DeclineChatJoinRequest {
    TelegramId chat_id{};
    TelegramId user_id{};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "chat_id", chat_id);
        json_put(object, "user_id", user_id);
        return JsonValue::object(std::move(object));
    }
};
