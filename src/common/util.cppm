export module common.util;

import std;

export auto escape_html(std::string_view text) -> std::string;

export auto escape_html_attribute(std::string_view text) -> std::string;

export auto encode_url_component(std::string_view text) -> std::string;

export auto sanitize_file_name(std::string_view name, std::string_view fallback) -> std::string;

export auto shell_quote(std::string_view value) -> std::string;

export auto now_millis() -> std::int64_t;
