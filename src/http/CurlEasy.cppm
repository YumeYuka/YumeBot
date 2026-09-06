module;

#include <curl/curl.h>

export module http.curl.easy;

import std;

export class CurlEasy {
public:
    CurlEasy();
    ~CurlEasy() = default;

    CurlEasy(CurlEasy &&) noexcept = default;
    CurlEasy &operator=(CurlEasy &&) noexcept = default;

    CurlEasy(const CurlEasy &) = delete;
    CurlEasy &operator=(const CurlEasy &) = delete;

    [[nodiscard]] auto get() const noexcept -> CURL *;

private:
    static auto cleanup(CURL *handle) noexcept -> void;

    std::unique_ptr<CURL, void (*)(CURL *)> handle_;
};
