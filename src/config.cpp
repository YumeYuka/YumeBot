module config;

import std;

static auto trim(std::string_view s) -> std::string_view {
    const auto first = s.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = s.find_last_not_of(" \t\r");
    return s.substr(first, last - first + 1);
}

static auto mask_secret(const std::string_view value) -> std::string {
    if (value.empty()) {
        return {};
    }
    if (value.size() <= 4) {
        return "****";
    }
    return std::string{"****"} + std::string{value.substr(value.size() - 4)};
}


auto Config::form_file(std::string_view path) -> Config {
    const std::filesystem::path file{path};
    auto in = std::ifstream{file};
    if (!in) {
        throw std::runtime_error("failed to open config file: " + file.string());
    }

    const std::string text{std::istreambuf_iterator{in}, std::istreambuf_iterator<char>{}};

    Config cfg;
    std::size_t start = 0;
    if (text.size() >= 3
        && static_cast<unsigned char>(text[0]) == 0xEF
        && static_cast<unsigned char>(text[1]) == 0xBB
        && static_cast<unsigned char>(text[2]) == 0xBF) {
        start = 3;
    }
    for (; start < text.size();) {
        auto end = text.find('\n', start);
        if (end == std::string::npos) {
            end = text.size();
        }

        const auto raw = trim(std::string_view{text}.substr(start, end - start));
        start = end + 1;

        if (raw.empty() || raw.starts_with('#')) {
            continue;
        }

        const auto eq = raw.find('=');
        if (eq == std::string_view::npos) {
            continue;
        }

        const auto key = trim(raw.substr(0, eq));
        const auto value = std::string{trim(raw.substr(eq + 1))};

        if (key == "bot_token") {
            cfg.bot_token_ = value;
        } else if (key == "mini_app_url") {
            cfg.mini_app_url_ = value;
        } else if (key == "app_id" || key == "api_id") {
            cfg.app_id_ = value;
        } else if (key == "api_hash") {
            cfg.api_hash_ = value;
        } else if (key == "bilibili_admin_id") {
            cfg.bilibili_admin_id_ = value;
        } else if (key == "netease_music_u") {
            cfg.netease_music_u_ = value;
        } else if (key == "telegram_api_base_url") {
            cfg.telegram_api_base_url_ = value;
        }
    }

    if (cfg.telegram_api_base_url_.empty()) {
        cfg.telegram_api_base_url_ = "https://api.telegram.org";
    }
    if (cfg.bot_token_.empty()) {
        throw std::runtime_error("config missing bot_token: " + file.string());
    }

    return cfg;
}

auto Config::to_string() const -> std::string {
    std::string out;
    out += "bot_token=";
    out += mask_secret(bot_token_);
    out += "\nmini_app_url=";
    out += mini_app_url_;
    out += "\napp_id=";
    out += app_id_;
    out += "\napi_hash=";
    out += mask_secret(api_hash_);
    out += "\nbilibili_admin_id=";
    out += bilibili_admin_id_;
    out += "\nnetease_music_u=";
    out += mask_secret(netease_music_u_);
    out += "\ntelegram_api_base_url=";
    out += telegram_api_base_url_;
    return out;
}
