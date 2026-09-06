module;

#include <curl/curl.h>

export module http.curl.headers;

import std;

export class CurlHeaders {
public:
    CurlHeaders() = default;
    ~CurlHeaders();

    CurlHeaders(const CurlHeaders &) = delete;
    CurlHeaders &operator=(const CurlHeaders &) = delete;

    CurlHeaders(CurlHeaders &&other) noexcept;
    CurlHeaders &operator=(CurlHeaders &&other) noexcept;

    auto append(std::string_view line) -> void;
    [[nodiscard]] auto get() const noexcept -> curl_slist *;

private:
    curl_slist *list_{nullptr};
};
