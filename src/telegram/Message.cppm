export module telegram.message;

import std;
import telegram.chat;
import telegram.json;
import telegram.types;
import telegram.user;

export struct WebAppData {
    std::string data{};
    std::string button_text{};

    static auto from_json(const JsonValue &json) -> WebAppData {
        return WebAppData{
            .data = json_string(json, "data").value_or(""),
            .button_text = json_string(json, "button_text").value_or(""),
        };
    }
};

export struct MessageEntity {
    std::string type{};
    int offset{0};
    int length{0};
    std::optional<std::string> url{};
    std::optional<User> user{};
    std::optional<std::string> language{};
    std::optional<std::string> custom_emoji_id{};

    static auto from_json(const JsonValue &json) -> MessageEntity {
        MessageEntity entity;
        entity.type = json_string(json, "type").value_or("");
        entity.offset = static_cast<int>(json_i64(json, "offset").value_or(0));
        entity.length = static_cast<int>(json_i64(json, "length").value_or(0));
        entity.url = json_string(json, "url");
        if (const auto *user = json_object(json, "user")) {
            entity.user = User::from_json(*user);
        }
        entity.language = json_string(json, "language");
        entity.custom_emoji_id = json_string(json, "custom_emoji_id");
        return entity;
    }
};

export struct Message {
    std::int64_t message_id{};
    std::optional<std::int64_t> message_thread_id{};
    std::optional<User> from{};
    UnixTime date{};
    Chat chat{};
    std::optional<std::string> text{};
    std::optional<std::vector<MessageEntity>> entities{};
    std::optional<std::unique_ptr<Message>> reply_to_message{};
    std::optional<WebAppData> web_app_data{};
    std::optional<std::vector<User>> new_chat_members{};
    std::optional<User> left_chat_member{};

    static auto from_json(const JsonValue &json) -> Message {
        Message message;
        message.message_id = json_i64(json, "message_id").value_or(0);
        message.message_thread_id = json_i64(json, "message_thread_id");
        if (const auto *from = json_object(json, "from")) {
            message.from = User::from_json(*from);
        }
        message.date = json_i64(json, "date").value_or(0);
        if (const auto *chat = json_object(json, "chat")) {
            message.chat = Chat::from_json(*chat);
        }
        message.text = json_string(json, "text");
        if (const auto *entities = json_array(json, "entities")) {
            std::vector<MessageEntity> parsed;
            parsed.reserve(entities->size());
            for (const auto &item : *entities) {
                parsed.push_back(MessageEntity::from_json(item));
            }
            message.entities = std::move(parsed);
        }
        if (const auto *reply = json_object(json, "reply_to_message")) {
            message.reply_to_message = std::make_unique<Message>(from_json(*reply));
        }
        if (const auto *web_app = json_object(json, "web_app_data")) {
            message.web_app_data = WebAppData::from_json(*web_app);
        }
        if (const auto *members = json_array(json, "new_chat_members")) {
            std::vector<User> parsed;
            parsed.reserve(members->size());
            for (const auto &item : *members) {
                parsed.push_back(User::from_json(item));
            }
            message.new_chat_members = std::move(parsed);
        }
        if (const auto *left = json_object(json, "left_chat_member")) {
            message.left_chat_member = User::from_json(*left);
        }
        return message;
    }
};
