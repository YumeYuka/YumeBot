module;

#include <curl/curl.h>

export module Http;

import std;
import CurlGlobal;
import CurlEasy;
import CurlHeaders;
import HttpResponse;

export class HttpClient {
public:
    HttpClient() { CurlGlobal::ensure_initialized(); }

    [[nodiscard]] auto get(std::string_view url) const -> HttpResponse;

    [[nodiscard]] auto post(
        std::string_view url,
        std::string_view body,
        std::span<const std::pair<std::string, std::string>> headers = {}
    ) const -> HttpResponse;

private:
    static auto write_to_string(
        char *contents,
        std::size_t size,
        std::size_t nmemb,
        void *userp
    ) -> std::size_t;

    static auto perform(CURL *curl) -> HttpResponse;
};

module :private;

auto HttpClient::write_to_string(
    char *contents,
    std::size_t size,
    std::size_t nmemb,
    void *userp
) -> std::size_t {
    const auto total = size * nmemb;
    static_cast<std::string *>(userp)->append(contents, total);
    return total;
}

auto HttpClient::perform(CURL *curl) -> HttpResponse {
    HttpResponse response;
    std::string buffer;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    const auto code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        response.error = curl_easy_strerror(code);
        return response;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
    response.body = std::move(buffer);
    return response;
}

auto HttpClient::get(std::string_view url) const -> HttpResponse {
    CurlEasy easy;
    const std::string url_str{url};

    curl_easy_setopt(easy.get(), CURLOPT_URL, url_str.c_str());

    return perform(easy.get());
}

auto HttpClient::post(
    std::string_view url,
    std::string_view body,
    std::span<const std::pair<std::string, std::string>> headers
) const -> HttpResponse {
    CurlEasy easy;
    CurlHeaders header_list;

    const std::string url_str{url};
    const std::string body_str{body};

    curl_easy_setopt(easy.get(), CURLOPT_URL, url_str.c_str());
    curl_easy_setopt(easy.get(), CURLOPT_POST, 1L);
    curl_easy_setopt(easy.get(), CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(easy.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(body_str.size()));

    for (const auto &[key, value] : headers) {
        header_list.append(key + ": " + value);
    }
    if (header_list.get() != nullptr) {
        curl_easy_setopt(easy.get(), CURLOPT_HTTPHEADER, header_list.get());
    }

    return perform(easy.get());
}
