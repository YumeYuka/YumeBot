module;

#include <curl/curl.h>

export module CurlEasy;

import std;

export class CurlEasy {
public:
    CurlEasy();
    ~CurlEasy() = default;

    CurlEasy(CurlEasy &&) noexcept = default;
    CurlEasy &operator=(CurlEasy &&) noexcept = default;

    CurlEasy(const CurlEasy &) = delete;
    CurlEasy &operator=(const CurlEasy &) = delete;

    [[nodiscard]] auto get() noexcept -> CURL *;

private:
    static auto cleanup(CURL *handle) noexcept -> void;

    std::unique_ptr<CURL, void (*)(CURL *)> handle_;
};

module :private;

auto CurlEasy::cleanup(CURL *handle) noexcept -> void {
    if (handle != nullptr) {
        curl_easy_cleanup(handle);
    }
}

CurlEasy::CurlEasy()
    : handle_{curl_easy_init(), cleanup} {
    if (!handle_) {
        throw std::runtime_error("curl_easy_init failed");
    }
}

auto CurlEasy::get() noexcept -> CURL * {
    return handle_.get();
}
