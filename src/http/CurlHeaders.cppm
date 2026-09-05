module;

#include <curl/curl.h>

export module CurlHeaders;

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

module :private;

CurlHeaders::~CurlHeaders() {
    if (list_ != nullptr) {
        curl_slist_free_all(list_);
    }
}

CurlHeaders::CurlHeaders(CurlHeaders &&other) noexcept
    : list_{other.list_} {
    other.list_ = nullptr;
}

CurlHeaders &CurlHeaders::operator=(CurlHeaders &&other) noexcept {
    if (this != &other) {
        if (list_ != nullptr) {
            curl_slist_free_all(list_);
        }
        list_ = other.list_;
        other.list_ = nullptr;
    }
    return *this;
}

auto CurlHeaders::append(std::string_view line) -> void {
    const std::string header_line{line};
    list_ = curl_slist_append(list_, header_line.c_str());
}

auto CurlHeaders::get() const noexcept -> curl_slist * {
    return list_;
}
