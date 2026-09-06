module telegram.json;

import std;

namespace {

auto json_escape(std::string_view input) -> std::string {
    std::string out;
    out.reserve(input.size() + 8);
    for (const unsigned char c : input) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20) {
                    constexpr char hex[] = "0123456789abcdef";
                    out += "\\u00";
                    out += hex[c >> 4];
                    out += hex[c & 0x0f];
                } else {
                    out += static_cast<char>(c);
                }
                break;
        }
    }
    return out;
}

class Parser {
public:
    explicit Parser(std::string_view text)
        : text_{text} {}

    auto parse() -> JsonValue {
        auto value = parse_value();
        skip();
        if (i_ != text_.size()) {
            throw std::runtime_error("telegram json: trailing data");
        }
        return value;
    }

private:
    std::string_view text_;
    std::size_t i_{0};

    auto skip() -> void {
        while (i_ < text_.size() && (text_[i_] == ' ' || text_[i_] == '\t' || text_[i_] == '\n' || text_[i_] == '\r')) {
            ++i_;
        }
    }

    auto peek() -> char {
        skip();
        return i_ < text_.size() ? text_[i_] : '\0';
    }

    auto take() -> char {
        skip();
        if (i_ >= text_.size()) {
            throw std::runtime_error("telegram json: unexpected end");
        }
        return text_[i_++];
    }

    auto expect(char c) -> void {
        if (take() != c) {
            throw std::runtime_error("telegram json: unexpected character");
        }
    }

    auto parse_value() -> JsonValue {
        switch (peek()) {
            case 'n':
                return parse_literal("null", JsonValue::null());
            case 't':
                return parse_literal("true", JsonValue::boolean(true));
            case 'f':
                return parse_literal("false", JsonValue::boolean(false));
            case '"':
                return JsonValue::string(parse_string());
            case '[':
                return parse_array();
            case '{':
                return parse_object();
            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                return JsonValue::number(parse_number());
            default:
                throw std::runtime_error("telegram json: invalid value");
        }
    }

    auto parse_literal(std::string_view literal, JsonValue value) -> JsonValue {
        if (!text_.substr(i_).starts_with(literal)) {
            throw std::runtime_error("telegram json: invalid literal");
        }
        i_ += literal.size();
        return value;
    }

    auto parse_string() -> std::string {
        expect('"');
        std::string out;
        while (i_ < text_.size()) {
            const auto c = text_[i_++];
            if (c == '"') {
                return out;
            }
            if (c != '\\') {
                out += c;
                continue;
            }
            if (i_ >= text_.size()) {
                throw std::runtime_error("telegram json: unterminated escape");
            }
            const auto e = text_[i_++];
            switch (e) {
                case '"':
                case '\\':
                case '/':
                    out += e;
                    break;
                case 'b':
                    out += '\b';
                    break;
                case 'f':
                    out += '\f';
                    break;
                case 'n':
                    out += '\n';
                    break;
                case 'r':
                    out += '\r';
                    break;
                case 't':
                    out += '\t';
                    break;
                case 'u': {
                    if (i_ + 4 > text_.size()) {
                        throw std::runtime_error("telegram json: bad unicode escape");
                    }
                    unsigned code = 0;
                    for (int n = 0; n < 4; ++n) {
                        const auto h = text_[i_++];
                        code <<= 4;
                        if (h >= '0' && h <= '9') {
                            code += static_cast<unsigned>(h - '0');
                        } else if (h >= 'a' && h <= 'f') {
                            code += static_cast<unsigned>(h - 'a' + 10);
                        } else if (h >= 'A' && h <= 'F') {
                            code += static_cast<unsigned>(h - 'A' + 10);
                        } else {
                            throw std::runtime_error("telegram json: bad unicode escape");
                        }
                    }
                    if (code < 0x80) {
                        out += static_cast<char>(code);
                    } else if (code < 0x800) {
                        out += static_cast<char>(0xc0 | (code >> 6));
                        out += static_cast<char>(0x80 | (code & 0x3f));
                    } else {
                        out += static_cast<char>(0xe0 | (code >> 12));
                        out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
                        out += static_cast<char>(0x80 | (code & 0x3f));
                    }
                    break;
                }
                default:
                    out += e;
                    break;
            }
        }
        throw std::runtime_error("telegram json: unterminated string");
    }

    auto parse_number() -> std::int64_t {
        skip();
        const auto begin = i_;
        if (peek() == '-') {
            ++i_;
        }
        if (i_ >= text_.size() || text_[i_] < '0' || text_[i_] > '9') {
            throw std::runtime_error("telegram json: invalid number");
        }
        while (i_ < text_.size() && text_[i_] >= '0' && text_[i_] <= '9') {
            ++i_;
        }
        const auto integer_end = i_;
        if (i_ < text_.size() && text_[i_] == '.') {
            ++i_;
            while (i_ < text_.size() && text_[i_] >= '0' && text_[i_] <= '9') {
                ++i_;
            }
        }
        if (i_ < text_.size() && (text_[i_] == 'e' || text_[i_] == 'E')) {
            ++i_;
            if (i_ < text_.size() && (text_[i_] == '+' || text_[i_] == '-')) {
                ++i_;
            }
            while (i_ < text_.size() && text_[i_] >= '0' && text_[i_] <= '9') {
                ++i_;
            }
        }
        return std::stoll(std::string{text_.substr(begin, integer_end - begin)});
    }

    auto parse_array() -> JsonValue {
        expect('[');
        JsonValue::Array items;
        skip();
        if (peek() == ']') {
            ++i_;
            return JsonValue::array(std::move(items));
        }
        while (true) {
            items.push_back(parse_value());
            const auto c = take();
            if (c == ']') {
                return JsonValue::array(std::move(items));
            }
            if (c != ',') {
                throw std::runtime_error("telegram json: expected comma in array");
            }
        }
    }

    auto parse_object() -> JsonValue {
        expect('{');
        JsonValue::Object object;
        skip();
        if (peek() == '}') {
            ++i_;
            return JsonValue::object(std::move(object));
        }
        while (true) {
            if (peek() != '"') {
                throw std::runtime_error("telegram json: expected object key");
            }
            auto key = parse_string();
            expect(':');
            object.emplace(std::move(key), parse_value());
            const auto c = take();
            if (c == '}') {
                return JsonValue::object(std::move(object));
            }
            if (c != ',') {
                throw std::runtime_error("telegram json: expected comma in object");
            }
        }
    }
};

auto dump_value(const JsonValue &value) -> std::string {
    switch (value.kind()) {
        case JsonValue::Kind::Null:
            return "null";
        case JsonValue::Kind::Bool:
            return *value.as_bool() ? "true" : "false";
        case JsonValue::Kind::Number:
            return std::to_string(*value.as_i64());
        case JsonValue::Kind::String:
            return '"' + json_escape(*value.as_string()) + '"';
        case JsonValue::Kind::Array: {
            std::string out = "[";
            const auto *items = value.as_array();
            for (std::size_t i = 0; i < items->size(); ++i) {
                if (i != 0) {
                    out += ',';
                }
                out += dump_value((*items)[i]);
            }
            out += ']';
            return out;
        }
        case JsonValue::Kind::Object: {
            std::string out = "{";
            bool first = true;
            for (const auto &[key, child] : *value.as_object()) {
                if (!first) {
                    out += ',';
                }
                first = false;
                out += '"';
                out += json_escape(key);
                out += "\":";
                out += dump_value(child);
            }
            out += '}';
            return out;
        }
    }
    return "null";
}

}  // namespace

auto JsonValue::parse(std::string_view text) -> JsonValue {
    return Parser{text}.parse();
}

auto JsonValue::dump() const -> std::string {
    return dump_value(*this);
}

auto json_string(const JsonValue &object, std::string_view key) -> std::optional<std::string> {
    const auto *value = object.get(key);
    if (value == nullptr) {
        return std::nullopt;
    }
    return value->as_string();
}

auto json_i64(const JsonValue &object, std::string_view key) -> std::optional<std::int64_t> {
    const auto *value = object.get(key);
    if (value == nullptr) {
        return std::nullopt;
    }
    return value->as_i64();
}

auto json_bool(const JsonValue &object, std::string_view key) -> std::optional<bool> {
    const auto *value = object.get(key);
    if (value == nullptr) {
        return std::nullopt;
    }
    return value->as_bool();
}

auto json_object(const JsonValue &object, std::string_view key) -> const JsonValue * {
    const auto *value = object.get(key);
    if (value == nullptr || value->as_object() == nullptr) {
        return nullptr;
    }
    return value;
}

auto json_array(const JsonValue &object, std::string_view key) -> const JsonValue::Array * {
    const auto *value = object.get(key);
    if (value == nullptr) {
        return nullptr;
    }
    return value->as_array();
}

auto json_put(JsonValue::Object &object, std::string_view key, const std::string &value) -> void {
    object.emplace(std::string{key}, JsonValue::string(value));
}

auto json_put(JsonValue::Object &object, std::string_view key, std::int64_t value) -> void {
    object.emplace(std::string{key}, JsonValue::number(value));
}

auto json_put(JsonValue::Object &object, std::string_view key, bool value) -> void {
    object.emplace(std::string{key}, JsonValue::boolean(value));
}

auto json_put(JsonValue::Object &object, std::string_view key, const JsonValue &value) -> void {
    object.emplace(std::string{key}, value);
}

auto json_put(JsonValue::Object &object, std::string_view key, const std::optional<std::string> &value) -> void {
    if (value.has_value()) {
        object.emplace(std::string{key}, JsonValue::string(*value));
    }
}

auto json_put(JsonValue::Object &object, std::string_view key, const std::optional<std::int64_t> &value) -> void {
    if (value.has_value()) {
        object.emplace(std::string{key}, JsonValue::number(*value));
    }
}

auto json_put(JsonValue::Object &object, std::string_view key, const std::optional<bool> &value) -> void {
    if (value.has_value()) {
        object.emplace(std::string{key}, JsonValue::boolean(*value));
    }
}

auto json_put(JsonValue::Object &object, std::string_view key, const std::optional<int> &value) -> void {
    if (value.has_value()) {
        object.emplace(std::string{key}, JsonValue::number(static_cast<std::int64_t>(*value)));
    }
}
