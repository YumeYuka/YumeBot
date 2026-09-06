export module telegram.request;

import std;
import telegram.json;
import telegram.markup;
import telegram.types;

export struct SendMessageRequest {
    TelegramId chat_id{};
    std::string text{};
    std::optional<std::string> parse_mode{};
    std::optional<std::int64_t> message_thread_id{};
    bool disable_web_page_preview{true};
    std::optional<ReplyMarkup> reply_markup{};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "chat_id", chat_id);
        json_put(object, "text", text);
        json_put(object, "parse_mode", parse_mode);
        json_put(object, "message_thread_id", message_thread_id);
        json_put(object, "disable_web_page_preview", disable_web_page_preview);
        if (reply_markup.has_value()) {
            json_put(object, "reply_markup", reply_markup_to_json(*reply_markup));
        }
        return JsonValue::object(std::move(object));
    }
};

export struct SendPhotoRequest {
    TelegramId chat_id{};
    std::string photo{};
    std::optional<std::string> caption{};
    std::optional<std::int64_t> message_thread_id{};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "chat_id", chat_id);
        json_put(object, "photo", photo);
        json_put(object, "caption", caption);
        json_put(object, "message_thread_id", message_thread_id);
        return JsonValue::object(std::move(object));
    }
};

export struct SendDocumentRequest {
    TelegramId chat_id{};
    std::string document{};
    std::optional<std::string> caption{};
    std::optional<std::int64_t> message_thread_id{};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "chat_id", chat_id);
        json_put(object, "document", document);
        json_put(object, "caption", caption);
        json_put(object, "message_thread_id", message_thread_id);
        return JsonValue::object(std::move(object));
    }
};

export struct DeleteMessageRequest {
    TelegramId chat_id{};
    std::int64_t message_id{};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "chat_id", chat_id);
        json_put(object, "message_id", message_id);
        return JsonValue::object(std::move(object));
    }
};

export struct AnswerCallbackQueryRequest {
    std::string callback_query_id{};
    std::optional<std::string> text{};
    std::optional<bool> show_alert{};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "callback_query_id", callback_query_id);
        json_put(object, "text", text);
        json_put(object, "show_alert", show_alert);
        return JsonValue::object(std::move(object));
    }
};
