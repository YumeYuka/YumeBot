export module telegram.user;

import std;
import telegram.json;
import telegram.types;

export struct User {
    TelegramId id{};
    bool is_bot{false};
    std::string first_name{};
    std::optional<std::string> last_name{};
    std::optional<std::string> username{};
    std::optional<std::string> language_code{};
    std::optional<bool> is_premium{};

    static auto from_json(const JsonValue &json) -> User {
        User user;
        user.id = json_i64(json, "id").value_or(0);
        user.is_bot = json_bool(json, "is_bot").value_or(false);
        user.first_name = json_string(json, "first_name").value_or("");
        user.last_name = json_string(json, "last_name");
        user.username = json_string(json, "username");
        user.language_code = json_string(json, "language_code");
        user.is_premium = json_bool(json, "is_premium");
        return user;
    }

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "id", id);
        json_put(object, "is_bot", is_bot);
        json_put(object, "first_name", first_name);
        json_put(object, "last_name", last_name);
        json_put(object, "username", username);
        json_put(object, "language_code", language_code);
        json_put(object, "is_premium", is_premium);
        return JsonValue::object(std::move(object));
    }
};

export struct BotCommand {
    std::string command{};
    std::string description{};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "command", command);
        json_put(object, "description", description);
        return JsonValue::object(std::move(object));
    }
};

export struct UserProfilePhotos {
    int total_count{0};

    static auto from_json(const JsonValue &json) -> UserProfilePhotos {
        return UserProfilePhotos{
            .total_count = static_cast<int>(json_i64(json, "total_count").value_or(0)),
        };
    }
};

export struct GetUserProfilePhotosRequest {
    TelegramId user_id{};
    int limit{1};

    [[nodiscard]] auto to_json() const -> JsonValue {
        JsonValue::Object object;
        json_put(object, "user_id", user_id);
        json_put(object, "limit", static_cast<std::int64_t>(limit));
        return JsonValue::object(std::move(object));
    }
};
