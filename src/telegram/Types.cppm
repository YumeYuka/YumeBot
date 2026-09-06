export module telegram.types;

import std;

export using TelegramId = std::int64_t;
export using UnixTime = std::int64_t;

export template<typename T>
struct TelegramResult {
    bool ok{false};
    std::optional<T> result{};
    std::optional<std::string> description{};
    std::optional<int> error_code{};
    std::string body{};

    [[nodiscard]] auto succeeded() const noexcept -> bool {
        return ok && result.has_value();
    }

    [[nodiscard]] auto error_text() const -> std::string {
        std::string text = description.value_or("unknown error");
        if (error_code.has_value()) {
            text += " (";
            text += std::to_string(*error_code);
            text += ")";
        }
        return text;
    }
};
