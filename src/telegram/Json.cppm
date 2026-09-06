export module telegram.json;

import std;

export class JsonValue {
public:
    enum class Kind {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object,
    };

    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;

    JsonValue() = default;

    static auto null() -> JsonValue { return {}; }

    static auto boolean(bool value) -> JsonValue {
        JsonValue out;
        out.kind_ = Kind::Bool;
        out.bool_ = value;
        return out;
    }

    static auto number(std::int64_t value) -> JsonValue {
        JsonValue out;
        out.kind_ = Kind::Number;
        out.number_ = value;
        return out;
    }

    static auto string(std::string value) -> JsonValue {
        JsonValue out;
        out.kind_ = Kind::String;
        out.string_ = std::move(value);
        return out;
    }

    static auto array(Array value) -> JsonValue {
        JsonValue out;
        out.kind_ = Kind::Array;
        out.array_ = std::move(value);
        return out;
    }

    static auto object(Object value) -> JsonValue {
        JsonValue out;
        out.kind_ = Kind::Object;
        out.object_ = std::move(value);
        return out;
    }

    static auto parse(std::string_view text) -> JsonValue;

    [[nodiscard]] auto kind() const noexcept -> Kind { return kind_; }
    [[nodiscard]] auto is_null() const noexcept -> bool { return kind_ == Kind::Null; }

    [[nodiscard]] auto as_bool() const -> std::optional<bool> {
        if (kind_ != Kind::Bool) {
            return std::nullopt;
        }
        return bool_;
    }

    [[nodiscard]] auto as_i64() const -> std::optional<std::int64_t> {
        if (kind_ != Kind::Number) {
            return std::nullopt;
        }
        return number_;
    }

    [[nodiscard]] auto as_string() const -> std::optional<std::string> {
        if (kind_ != Kind::String) {
            return std::nullopt;
        }
        return string_;
    }

    [[nodiscard]] auto as_array() const -> const Array * {
        if (kind_ != Kind::Array) {
            return nullptr;
        }
        return &array_;
    }

    [[nodiscard]] auto as_object() const -> const Object * {
        if (kind_ != Kind::Object) {
            return nullptr;
        }
        return &object_;
    }

    [[nodiscard]] auto get(std::string_view key) const -> const JsonValue * {
        if (kind_ != Kind::Object) {
            return nullptr;
        }
        const auto it = object_.find(std::string{key});
        if (it == object_.end()) {
            return nullptr;
        }
        return &it->second;
    }

    [[nodiscard]] auto dump() const -> std::string;

private:
    Kind kind_{Kind::Null};
    bool bool_{false};
    std::int64_t number_{0};
    std::string string_{};
    Array array_{};
    Object object_{};
};

export auto json_string(const JsonValue &object, std::string_view key) -> std::optional<std::string>;
export auto json_i64(const JsonValue &object, std::string_view key) -> std::optional<std::int64_t>;
export auto json_bool(const JsonValue &object, std::string_view key) -> std::optional<bool>;
export auto json_object(const JsonValue &object, std::string_view key) -> const JsonValue *;
export auto json_array(const JsonValue &object, std::string_view key) -> const JsonValue::Array *;

export auto json_put(JsonValue::Object &object, std::string_view key, const std::string &value) -> void;
export auto json_put(JsonValue::Object &object, std::string_view key, std::int64_t value) -> void;
export auto json_put(JsonValue::Object &object, std::string_view key, bool value) -> void;
export auto json_put(JsonValue::Object &object, std::string_view key, const JsonValue &value) -> void;
export auto json_put(JsonValue::Object &object, std::string_view key, const std::optional<std::string> &value) -> void;
export auto json_put(JsonValue::Object &object, std::string_view key, const std::optional<std::int64_t> &value) -> void;
export auto json_put(JsonValue::Object &object, std::string_view key, const std::optional<bool> &value) -> void;
export auto json_put(JsonValue::Object &object, std::string_view key, const std::optional<int> &value) -> void;
