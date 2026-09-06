export module telegram.update;

import std;
import telegram.join;
import telegram.json;
import telegram.member;
import telegram.message;
import telegram.types;
import telegram.user;

export struct CallbackQuery {
    std::string id{};
    User from{};
    std::optional<Message> message{};
    std::optional<std::string> inline_message_id{};
    std::string chat_instance{};
    std::optional<std::string> data{};

    static auto from_json(const JsonValue &json) -> CallbackQuery {
        CallbackQuery query;
        query.id = json_string(json, "id").value_or("");
        if (const auto *from = json_object(json, "from")) {
            query.from = User::from_json(*from);
        }
        if (const auto *message = json_object(json, "message")) {
            query.message = Message::from_json(*message);
        }
        query.inline_message_id = json_string(json, "inline_message_id");
        query.chat_instance = json_string(json, "chat_instance").value_or("");
        query.data = json_string(json, "data");
        return query;
    }
};

export struct Update {
    std::int64_t update_id{};
    std::optional<Message> message{};
    std::optional<ChatMemberUpdated> chat_member{};
    std::optional<ChatMemberUpdated> my_chat_member{};
    std::optional<ChatJoinRequest> chat_join_request{};
    std::optional<CallbackQuery> callback_query{};

    static auto from_json(const JsonValue &json) -> Update {
        Update update;
        update.update_id = json_i64(json, "update_id").value_or(0);
        if (const auto *message = json_object(json, "message")) {
            update.message = Message::from_json(*message);
        }
        if (const auto *member = json_object(json, "chat_member")) {
            update.chat_member = ChatMemberUpdated::from_json(*member);
        }
        if (const auto *mine = json_object(json, "my_chat_member")) {
            update.my_chat_member = ChatMemberUpdated::from_json(*mine);
        }
        if (const auto *join = json_object(json, "chat_join_request")) {
            update.chat_join_request = ChatJoinRequest::from_json(*join);
        }
        if (const auto *callback = json_object(json, "callback_query")) {
            update.callback_query = CallbackQuery::from_json(*callback);
        }
        return update;
    }
};

export struct GetUpdatesRequest {
    std::optional<std::int64_t> offset{};
    std::optional<int> limit{100};
    std::optional<int> timeout{30};
    std::vector<std::string> allowed_updates{
        "message",
        "chat_member",
        "chat_join_request",
        "callback_query",
    };

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "offset", offset);
        json_put(object, "limit", limit);
        json_put(object, "timeout", timeout);
        if (!allowed_updates.empty()) {
            JsonValue::Array updates;
            for (const auto &name : allowed_updates) {
                updates.push_back(JsonValue::string(name));
            }
            json_put(object, "allowed_updates", JsonValue::array(std::move(updates)));
        }
        return JsonValue::object(std::move(object));
    }
};
