export module telegram.markup;

import std;
import telegram.json;

export struct WebAppInfo {
    std::string url{};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "url", url);
        return JsonValue::object(std::move(object));
    }
};

export struct KeyboardButton {
    std::string text{};
    std::optional<WebAppInfo> web_app{};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "text", text);
        if (web_app.has_value()) {
            json_put(object, "web_app", web_app->to_json());
        }
        return JsonValue::object(std::move(object));
    }
};

export struct ReplyKeyboardMarkup {
    std::vector<std::vector<KeyboardButton>> keyboard{};
    bool resize_keyboard{true};
    bool one_time_keyboard{true};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Array rows;
        for (const auto &row : keyboard) {
            JsonValue::Array buttons;
            for (const auto &button : row) {
                buttons.push_back(button.to_json());
            }
            rows.push_back(JsonValue::array(std::move(buttons)));
        }
        JsonValue::Object object;
        json_put(object, "keyboard", JsonValue::array(std::move(rows)));
        json_put(object, "resize_keyboard", resize_keyboard);
        json_put(object, "one_time_keyboard", one_time_keyboard);
        return JsonValue::object(std::move(object));
    }
};

export struct ReplyKeyboardRemove {
    bool remove_keyboard{true};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "remove_keyboard", remove_keyboard);
        return JsonValue::object(std::move(object));
    }
};

export struct InlineKeyboardButton {
    std::string text{};
    std::optional<std::string> url{};
    std::optional<std::string> callback_data{};
    std::optional<WebAppInfo> web_app{};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "text", text);
        json_put(object, "url", url);
        json_put(object, "callback_data", callback_data);
        if (web_app.has_value()) {
            json_put(object, "web_app", web_app->to_json());
        }
        return JsonValue::object(std::move(object));
    }
};

export struct InlineKeyboardMarkup {
    std::vector<std::vector<InlineKeyboardButton>> inline_keyboard{};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Array rows;
        for (const auto &row : inline_keyboard) {
            JsonValue::Array buttons;
            for (const auto &button : row) {
                buttons.push_back(button.to_json());
            }
            rows.push_back(JsonValue::array(std::move(buttons)));
        }
        JsonValue::Object object;
        json_put(object, "inline_keyboard", JsonValue::array(std::move(rows)));
        return JsonValue::object(std::move(object));
    }
};

export using ReplyMarkup = std::variant<InlineKeyboardMarkup, ReplyKeyboardMarkup, ReplyKeyboardRemove>;

export auto reply_markup_to_json(const ReplyMarkup &markup) -> JsonValue {
    return std::visit([](const auto &value) { return value.to_json(); }, markup);
}
