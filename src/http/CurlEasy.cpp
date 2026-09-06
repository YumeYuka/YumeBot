module;

#include <curl/curl.h>

module http.curl.easy;

import std;

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

auto CurlEasy::get() const noexcept -> CURL * {
    return handle_.get();
}
