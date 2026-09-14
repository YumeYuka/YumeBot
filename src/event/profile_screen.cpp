module event.profile_screen;

import std;
import telegram.user;

namespace {

constexpr std::array<std::string_view, 50> k_scam_keywords = {
    "\u65e5\u7ed3", "\u65e5\u8d5a", "\u6708\u5165", "\u5237\u5355", "\u517c\u804c",
    "\u517c\u8077", "\u7f51\u8d37", "\u8d37\u6b3e", "\u8cb8\u6b3e", "\u535a\u5f69",
    "\u8d4c\u535a", "\u8ced\u535a", "\u4ee3\u5f00", "\u53d1\u7968", "\u5f15\u6d41",
    "\u4ee3\u7406", "\u9ad8\u85aa", "\u8f7b\u677e\u8d5a", "\u5728\u5bb6\u505a", "\u5957\u73b0",
    "\u6d17\u94b1", "\u8fd4\u5229", "\u6295\u8d44\u7406\u8d22", "\u68cb\u724c", "\u529e\u8bc1",
    "\u9ad8\u4eff", "\u7ea6\u70ae", "\u770b\u7247", "USDT", "usdt",
    "\u7a7a\u6295", "\u64b8\u6bdb", "\u8585\u7f8a\u6bdb", "\u63a5\u7801", "\u5361\u5546",
    "\u6599\u5b50", "\u62c5\u4fdd", "\u4ee3\u5145", "\u4e0a\u5206", "\u4e0b\u5206",
    "\u5e26\u5355", "\u6740\u732a\u76d8",
    "\u8d5a", "\u8d4c", "\u8d37", "\u5237", "\u5ad6", "\u70ae", "\u6deb", "\u6bd2",
};

constexpr std::array<std::string_view, 4> k_garbled_markers = {
    "\u951f\u65a4\u62f7", "\u70eb\u70eb\u70eb", "\u5c6f\u5c6f\u5c6f", "\u00ef\u00b8",
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

auto utf8_append(std::string &out, char32_t codepoint) -> void {
    if (codepoint < 0x80) {
        out.push_back(static_cast<char>(codepoint));
        return;
    }
    if (codepoint < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    if (codepoint < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
}

auto is_ignored_codepoint(char32_t codepoint) -> bool {
    if (codepoint == 0x00AD || codepoint == 0x034F || codepoint == 0x061C
        || codepoint == 0x180E || codepoint == 0xFEFF) {
        return true;
    }
    if (codepoint >= 0x200B && codepoint <= 0x200F) {
        return true;
    }
    if (codepoint >= 0x202A && codepoint <= 0x202E) {
        return true;
    }
    if (codepoint >= 0x2060 && codepoint <= 0x206F) {
        return true;
    }
    if (codepoint >= 0xFE00 && codepoint <= 0xFE0F) {
        return true;
    }
    if (codepoint >= 0xE0100 && codepoint <= 0xE01EF) {
        return true;
    }
    if (codepoint >= 0x0300 && codepoint <= 0x036F) {
        return true;
    }
    return false;
}

auto fold_codepoint(char32_t codepoint) -> char32_t {
    if (codepoint >= U'A' && codepoint <= U'Z') {
        return codepoint + 32;
    }
    if (codepoint >= 0xFF01 && codepoint <= 0xFF5E) {
        return fold_codepoint(codepoint - 0xFEE0);
    }
    switch (codepoint) {
        case 0x8CFA: return 0x8D5A; // 賺 -> 赚
        case 0x8CED: return 0x8D4C; // 賭 -> 赌
        case 0x8CB8: return 0x8D37; // 貸 -> 贷
        case 0x55AE: return 0x5355; // 單 -> 单
        case 0x7DB2: return 0x7F51; // 網 -> 网
        case 0x8B49: return 0x8BC1; // 證 -> 证
        case 0x767C: return 0x53D1; // 發 -> 发
        case 0x8077: return 0x804C; // 職 -> 职
        case 0x9322: return 0x94B1; // 錢 -> 钱
        case 0x5E63: return 0x5E01; // 幣 -> 币
        case 0x7121: return 0x65E0; // 無 -> 无
        default: return codepoint;
    }
}

auto normalize_for_match(std::string_view text) -> std::string {
    std::string out;
    out.reserve(text.size());
    std::size_t index = 0;
    while (index < text.size()) {
        const auto saved = index;
        const auto codepoint = utf8_decode(text, index);
        if (!codepoint.has_value()) {
            index = saved + 1;
            continue;
        }
        if (is_ignored_codepoint(*codepoint)) {
            continue;
        }
        utf8_append(out, fold_codepoint(*codepoint));
    }
    return out;
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
    const auto haystack = normalize_for_match(text);
    if (haystack.empty()) {
        return false;
    }
    for (const auto keyword : k_scam_keywords) {
        const auto needle = normalize_for_match(keyword);
        if (!needle.empty() && haystack.contains(needle)) {
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

auto build_keyword_haystack(const ProfileScreenInput &input, std::string_view display_name) -> std::string {
    std::string haystack{display_name};
    haystack += '\n';
    haystack += input.user.first_name;
    if (input.user.last_name.has_value()) {
        haystack += '\n';
        haystack += *input.user.last_name;
    }
    if (input.user.username.has_value()) {
        haystack += '\n';
        haystack += *input.user.username;
    }
    if (input.bio.has_value()) {
        haystack += '\n';
        haystack += *input.bio;
    }
    return haystack;
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
    if (input.bio.has_value() && !input.bio->empty() && is_garbled_text(*input.bio)) {
        return ProfileScreenResult{.blocked = true, .reason = "garbled bio"};
    }
    if (contains_scam_keyword(build_keyword_haystack(input, display_name))) {
        return ProfileScreenResult{.blocked = true, .reason = "scam keyword"};
    }

    return {};
}
