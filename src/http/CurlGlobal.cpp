module;

#include <curl/curl.h>

module http.curl.global;

import std;

auto CurlGlobal::ensure_initialized() -> void {
    static const CurlGlobal instance;
}

CurlGlobal::CurlGlobal() {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        throw std::runtime_error("curl_global_init failed");
    }
}

CurlGlobal::~CurlGlobal() {
    curl_global_cleanup();
}
