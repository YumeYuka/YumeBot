export module telegram.chat;

import std;
import telegram.json;
import telegram.types;

export struct Chat {
    TelegramId id{};
    std::string type{};
    std::optional<std::string> title{};
    std::optional<std::string> username{};
    std::optional<std::string> first_name{};
    std::optional<std::string> last_name{};

    static auto from_json(const JsonValue &json) -> Chat {
        Chat chat;
        chat.id = json_i64(json, "id").value_or(0);
        chat.type = json_string(json, "type").value_or("");
        chat.title = json_string(json, "title");
        chat.username = json_string(json, "username");
        chat.first_name = json_string(json, "first_name");
        chat.last_name = json_string(json, "last_name");
        return chat;
    }
};
