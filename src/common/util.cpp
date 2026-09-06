module common.util;

import std;

auto escape_html(std::string_view text) -> std::string {
    std::string escaped;
    escaped.reserve(text.size());
    for (const char ch : text) {
        switch (ch) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            default: escaped += ch; break;
        }
    }
    return escaped;
}

auto escape_html_attribute(std::string_view text) -> std::string {
    auto escaped = escape_html(text);
    std::replace(escaped.begin(), escaped.end(), '"', '\'');
    return escaped;
}

auto encode_url_component(std::string_view text) -> std::string {
    static constexpr auto is_unreserved = [](unsigned char ch) -> bool {
        return std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~';
    };

    std::string encoded;
    encoded.reserve(text.size() * 3);
    for (unsigned char ch : text) {
        if (is_unreserved(ch)) {
            encoded += static_cast<char>(ch);
            continue;
        }
        constexpr char hex[] = "0123456789ABCDEF";
        encoded += '%';
        encoded += hex[ch >> 4];
        encoded += hex[ch & 0x0F];
    }
    return encoded;
}

auto sanitize_file_name(std::string_view name, std::string_view fallback) -> std::string {
    std::string sanitized;
    sanitized.reserve(name.size());
    for (const char ch : name) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '.' || ch == '_' || ch == '-' || ch == ' ') {
            sanitized += ch;
        } else {
            sanitized += '_';
        }
    }
    while (!sanitized.empty() && sanitized.front() == ' ') {
        sanitized.erase(sanitized.begin());
    }
    while (!sanitized.empty() && sanitized.back() == ' ') {
        sanitized.pop_back();
    }
    if (sanitized.size() > 80) {
        sanitized.resize(80);
    }
    if (sanitized.empty()) {
        return std::string{fallback};
    }
    return sanitized;
}

auto shell_quote(std::string_view value) -> std::string {
#ifdef _WIN32
    std::string quoted{"\""};
    for (const char ch : value) {
        if (ch == '"') {
            quoted += "\\\"";
        } else {
            quoted += ch;
        }
    }
    quoted += '"';
    return quoted;
#else
    std::string quoted{"'"};
    for (const char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += '\'';
    return quoted;
#endif
}

auto now_millis() -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}
