module common.util;

import std;

namespace {

auto utf8_sequence_length(std::string_view text, std::size_t i) -> std::size_t {
    if (i >= text.size()) {
        return 0;
    }
    const auto lead = static_cast<unsigned char>(text[i]);
    std::size_t need = 0;
    if (lead < 0x80) {
        return 1;
    }
    if ((lead & 0xE0) == 0xC0 && lead >= 0xC2) {
        need = 2;
    } else if ((lead & 0xF0) == 0xE0) {
        need = 3;
    } else if ((lead & 0xF8) == 0xF0 && lead <= 0xF4) {
        need = 4;
    } else {
        return 0;
    }
    if (i + need > text.size()) {
        return 0;
    }
    for (std::size_t n = 1; n < need; ++n) {
        if ((static_cast<unsigned char>(text[i + n]) & 0xC0) != 0x80) {
            return 0;
        }
    }
    if (need == 3) {
        const auto b1 = static_cast<unsigned char>(text[i + 1]);
        if (lead == 0xE0 && b1 < 0xA0) {
            return 0;
        }
        if (lead == 0xED && b1 >= 0xA0) {
            return 0;
        }
    }
    if (need == 4) {
        const auto b1 = static_cast<unsigned char>(text[i + 1]);
        if (lead == 0xF0 && b1 < 0x90) {
            return 0;
        }
        if (lead == 0xF4 && b1 >= 0x90) {
            return 0;
        }
    }
    return need;
}

}  // namespace

auto sanitize_utf8(std::string_view text) -> std::string {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        const auto len = utf8_sequence_length(text, i);
        if (len == 0) {
            out += "\xEF\xBF\xBD";
            ++i;
            continue;
        }
        out.append(text.substr(i, len));
        i += len;
    }
    return out;
}

auto truncate_utf8(std::string_view text, std::size_t max_bytes) -> std::string {
    const auto clean = sanitize_utf8(text);
    if (clean.size() <= max_bytes) {
        return clean;
    }
    std::size_t i = 0;
    while (i < clean.size()) {
        const auto len = utf8_sequence_length(clean, i);
        if (len == 0 || i + len > max_bytes) {
            break;
        }
        i += len;
    }
    return std::string{clean.substr(0, i)};
}

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
