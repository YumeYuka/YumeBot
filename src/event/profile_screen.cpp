module event.profile_screen;

import std;
import telegram.user;

namespace {

constexpr std::array<std::string_view, 30> k_scam_keywords = {
    "日结", "日赚", "月入", "刷单", "兼职", "兼職", "网贷", "贷款", "貸款",
    "博彩", "赌博", "賭博", "代开", "发票", "引流", "代理", "高薪", "轻松赚",
    "在家做", "套现", "洗钱", "返利", "投资理财", "棋牌", "办证", "高仿",
    "约炮", "看片", "USDT", "usdt",
};

constexpr std::array<std::string_view, 4> k_garbled_markers = {
    "锟斤拷", "烫烫烫", "屯屯屯", "ï¸",
};

auto trim_text(std::string_view text) -> std::string_view {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return text;
}

auto utf8_decode(std::string_view text, std::size_t &index) -> std::optional<char32_t> {
    if (index >= text.size()) {
        return std::nullopt;
    }
    const auto b0 = static_cast<unsigned char>(text[index++]);
    if (b0 < 0x80) {
        return static_cast<char32_t>(b0);
    }
    if ((b0 & 0xE0) == 0xC0 && index < text.size()) {
        const auto b1 = static_cast<unsigned char>(text[index++]);
        return static_cast<char32_t>(((b0 & 0x1F) << 6) | (b1 & 0x3F));
    }
    if ((b0 & 0xF0) == 0xE0 && index + 1 < text.size()) {
        const auto b1 = static_cast<unsigned char>(text[index++]);
        const auto b2 = static_cast<unsigned char>(text[index++]);
        return static_cast<char32_t>(((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F));
    }
    if ((b0 & 0xF8) == 0xF0 && index + 2 < text.size()) {
        const auto b1 = static_cast<unsigned char>(text[index++]);
        const auto b2 = static_cast<unsigned char>(text[index++]);
        const auto b3 = static_cast<unsigned char>(text[index++]);
        return static_cast<char32_t>(
            ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F)
        );
    }
    return std::nullopt;
}

auto is_meaningful_char(char32_t codepoint) -> bool {
    if (codepoint == 0xFFFD) {
        return false;
    }
    if (codepoint >= 0x30 && codepoint <= 0x39) {
        return true;
    }
    if ((codepoint >= 'A' && codepoint <= 'Z') || (codepoint >= 'a' && codepoint <= 'z')) {
        return true;
    }
    if (codepoint >= 0x4E00 && codepoint <= 0x9FFF) {
        return true;
    }
    if (codepoint >= 0x3400 && codepoint <= 0x4DBF) {
        return true;
    }
    if (codepoint >= 0x3040 && codepoint <= 0x30FF) {
        return true;
    }
    if (codepoint >= 0xAC00 && codepoint <= 0xD7AF) {
        return true;
    }
    if (codepoint >= 0x0400 && codepoint <= 0x04FF) {
        return true;
    }
    return false;
}

auto is_garbled_text(std::string_view text) -> bool {
    const auto trimmed = trim_text(text);
    if (trimmed.empty()) {
        return true;
    }

    for (const auto marker : k_garbled_markers) {
        if (trimmed.contains(marker)) {
            return true;
        }
    }

    std::size_t index = 0;
    std::size_t meaningful = 0;
    std::size_t total = 0;
    std::size_t latin_supplement = 0;
    while (index < trimmed.size()) {
        const auto codepoint = utf8_decode(trimmed, index);
        if (!codepoint.has_value()) {
            return true;
        }
        ++total;
        if (is_meaningful_char(*codepoint)) {
            ++meaningful;
        }
        if (*codepoint >= 0x00C0 && *codepoint <= 0x024F) {
            ++latin_supplement;
        }
    }

    if (meaningful == 0) {
        return true;
    }
    if (total >= 4 && latin_supplement * 2 >= total) {
        return true;
    }
    return false;
}

auto contains_scam_keyword(std::string_view text) -> bool {
    if (text.empty()) {
        return false;
    }
    std::string lowered{text};
    std::ranges::transform(lowered, lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    for (const auto keyword : k_scam_keywords) {
        if (text.contains(keyword) || lowered.contains(keyword)) {
            return true;
        }
    }
    return false;
}

auto build_display_name(const User &user) -> std::string {
    std::string name = user.first_name;
    if (user.last_name.has_value() && !user.last_name->empty()) {
        if (!name.empty()) {
            name += ' ';
        }
        name += *user.last_name;
    }
    if (user.username.has_value() && !user.username->empty()) {
        if (!name.empty()) {
            name += ' ';
        }
        name += '@';
        name += *user.username;
    }
    return name;
}

}  // namespace

auto ProfileScreen::evaluate(const ProfileScreenInput &input) -> ProfileScreenResult {
    if (!input.has_avatar) {
        return ProfileScreenResult{.blocked = true, .reason = "missing avatar"};
    }

    const auto display_name = build_display_name(input.user);
    if (is_garbled_text(display_name) || is_garbled_text(input.user.first_name)) {
        return ProfileScreenResult{.blocked = true, .reason = "garbled display name"};
    }
    if (input.user.last_name.has_value() && is_garbled_text(*input.user.last_name)) {
        return ProfileScreenResult{.blocked = true, .reason = "garbled last name"};
    }
    if (input.user.username.has_value() && is_garbled_text(*input.user.username)) {
        return ProfileScreenResult{.blocked = true, .reason = "garbled username"};
    }

    if (contains_scam_keyword(display_name)) {
        return ProfileScreenResult{.blocked = true, .reason = "scam keyword in display name"};
    }
    if (input.user.username.has_value() && contains_scam_keyword(*input.user.username)) {
        return ProfileScreenResult{.blocked = true, .reason = "scam keyword in username"};
    }

    if (input.bio.has_value() && !input.bio->empty()) {
        if (is_garbled_text(*input.bio)) {
            return ProfileScreenResult{.blocked = true, .reason = "garbled bio"};
        }
        if (contains_scam_keyword(*input.bio)) {
            return ProfileScreenResult{.blocked = true, .reason = "scam keyword in bio"};
        }
    }

    return {};
}
