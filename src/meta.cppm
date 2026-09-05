export module meta;

import std;

using namespace std::literals::string_view_literals;

export struct Meta {
    static constexpr auto version = 1;

    static constexpr auto author = "YumeYuka"sv;

    static constexpr auto description = "A Telegram bot for verifying new group members via a Telegram Mini App."sv;

    static constexpr auto license = "BSD-3-Clause license"sv;

    static constexpr auto verify_link = "https://verify.yumeyuka.moe"sv;
};
