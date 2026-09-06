export module config;

import std;

export class Config {
public:
    static auto form_file(std::string_view path = "config.conf") -> Config;

    [[nodiscard]] auto to_string() const -> std::string;

    [[nodiscard]] auto bot_token() const -> const std::string & { return bot_token_; }
    [[nodiscard]] auto mini_app_url() const -> const std::string & { return mini_app_url_; }
    [[nodiscard]] auto app_id() const -> const std::string & { return app_id_; }
    [[nodiscard]] auto api_hash() const -> const std::string & { return api_hash_; }
    [[nodiscard]] auto bilibili_admin_id() const -> const std::string & { return bilibili_admin_id_; }
    [[nodiscard]] auto netease_music_u() const -> const std::string & { return netease_music_u_; }
    [[nodiscard]] auto telegram_api_base_url() const -> const std::string & { return telegram_api_base_url_; }

private:
    std::string bot_token_;
    std::string mini_app_url_;
    std::string app_id_;
    std::string api_hash_;
    std::string bilibili_admin_id_;
    std::string netease_music_u_;
    std::string telegram_api_base_url_{"https://api.telegram.org"};
};