export module telegram.member;

import std;
import telegram.chat;
import telegram.join;
import telegram.json;
import telegram.types;
import telegram.user;

export struct ChatPermissions {
    std::optional<bool> can_send_messages{};
    std::optional<bool> can_send_audios{};
    std::optional<bool> can_send_documents{};
    std::optional<bool> can_send_photos{};
    std::optional<bool> can_send_videos{};
    std::optional<bool> can_send_video_notes{};
    std::optional<bool> can_send_voice_notes{};
    std::optional<bool> can_send_polls{};
    std::optional<bool> can_send_other_messages{};
    std::optional<bool> can_add_web_page_previews{};
    std::optional<bool> can_change_info{};
    std::optional<bool> can_invite_users{};
    std::optional<bool> can_pin_messages{};
    std::optional<bool> can_manage_topics{};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "can_send_messages", can_send_messages);
        json_put(object, "can_send_audios", can_send_audios);
        json_put(object, "can_send_documents", can_send_documents);
        json_put(object, "can_send_photos", can_send_photos);
        json_put(object, "can_send_videos", can_send_videos);
        json_put(object, "can_send_video_notes", can_send_video_notes);
        json_put(object, "can_send_voice_notes", can_send_voice_notes);
        json_put(object, "can_send_polls", can_send_polls);
        json_put(object, "can_send_other_messages", can_send_other_messages);
        json_put(object, "can_add_web_page_previews", can_add_web_page_previews);
        json_put(object, "can_change_info", can_change_info);
        json_put(object, "can_invite_users", can_invite_users);
        json_put(object, "can_pin_messages", can_pin_messages);
        json_put(object, "can_manage_topics", can_manage_topics);
        return JsonValue::object(std::move(object));
    }
};

export struct ChatMember {
    std::string status{};
    User user{};
    std::optional<UnixTime> until_date{};
    std::optional<bool> is_member{};
    std::optional<bool> can_restrict_members{};
    std::optional<bool> can_delete_messages{};
    std::optional<bool> can_invite_users{};

    static auto from_json(const JsonValue &json) -> ChatMember {
        ChatMember member;
        member.status = json_string(json, "status").value_or("");
        if (const auto *user = json_object(json, "user")) {
            member.user = User::from_json(*user);
        }
        member.until_date = json_i64(json, "until_date");
        member.is_member = json_bool(json, "is_member");
        member.can_restrict_members = json_bool(json, "can_restrict_members");
        member.can_delete_messages = json_bool(json, "can_delete_messages");
        member.can_invite_users = json_bool(json, "can_invite_users");
        return member;
    }
};

export struct ChatMemberUpdated {
    Chat chat{};
    User from{};
    UnixTime date{};
    ChatMember old_chat_member{};
    ChatMember new_chat_member{};
    std::optional<ChatInviteLink> invite_link{};
    std::optional<bool> via_join_request{};
    std::optional<bool> via_chat_folder_invite_link{};

    static auto from_json(const JsonValue &json) -> ChatMemberUpdated {
        ChatMemberUpdated updated;
        if (const auto *chat = json_object(json, "chat")) {
            updated.chat = Chat::from_json(*chat);
        }
        if (const auto *from = json_object(json, "from")) {
            updated.from = User::from_json(*from);
        }
        updated.date = json_i64(json, "date").value_or(0);
        if (const auto *old_member = json_object(json, "old_chat_member")) {
            updated.old_chat_member = ChatMember::from_json(*old_member);
        }
        if (const auto *new_member = json_object(json, "new_chat_member")) {
            updated.new_chat_member = ChatMember::from_json(*new_member);
        }
        if (const auto *link = json_object(json, "invite_link")) {
            updated.invite_link = ChatInviteLink::from_json(*link);
        }
        updated.via_join_request = json_bool(json, "via_join_request");
        updated.via_chat_folder_invite_link = json_bool(json, "via_chat_folder_invite_link");
        return updated;
    }
};

export struct GetChatMemberRequest {
    TelegramId chat_id{};
    TelegramId user_id{};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "chat_id", chat_id);
        json_put(object, "user_id", user_id);
        return JsonValue::object(std::move(object));
    }
};

export struct BanChatMemberRequest {
    TelegramId chat_id{};
    TelegramId user_id{};
    std::optional<UnixTime> until_date{};
    std::optional<bool> revoke_messages{true};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "chat_id", chat_id);
        json_put(object, "user_id", user_id);
        json_put(object, "until_date", until_date);
        json_put(object, "revoke_messages", revoke_messages);
        return JsonValue::object(std::move(object));
    }
};

export struct UnbanChatMemberRequest {
    TelegramId chat_id{};
    TelegramId user_id{};
    std::optional<bool> only_if_banned{true};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "chat_id", chat_id);
        json_put(object, "user_id", user_id);
        json_put(object, "only_if_banned", only_if_banned);
        return JsonValue::object(std::move(object));
    }
};

export struct RestrictChatMemberRequest {
    TelegramId chat_id{};
    TelegramId user_id{};
    ChatPermissions permissions{};
    std::optional<UnixTime> until_date{};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "chat_id", chat_id);
        json_put(object, "user_id", user_id);
        json_put(object, "permissions", permissions.to_json());
        json_put(object, "until_date", until_date);
        return JsonValue::object(std::move(object));
    }
};
