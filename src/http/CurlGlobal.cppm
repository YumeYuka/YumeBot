module;

#include <curl/curl.h>

export module CurlGlobal;

import std;

export class CurlGlobal {
public:
    static auto ensure_initialized() -> void;

    CurlGlobal(const CurlGlobal &) = delete;
    CurlGlobal &operator=(const CurlGlobal &) = delete;

private:
    CurlGlobal();
    ~CurlGlobal();
};

module :private;

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
