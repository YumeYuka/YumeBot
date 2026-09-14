module;

#include <curl/curl.h>

module http;

import std;
import http.curl.global;
import http.curl.easy;
import http.curl.headers;
import http.response;

HttpClient::HttpClient() {
    CurlGlobal::ensure_initialized();
}

namespace {

auto write_to_string(
    char *contents,
    std::size_t size,
    std::size_t nmemb,
    void *userp
) -> std::size_t {
    const auto total = size * nmemb;
    static_cast<std::string *>(userp)->append(contents, total);
    return total;
}

auto header_callback(char *buffer, std::size_t size, std::size_t nitems, void *userdata) -> std::size_t {
    const auto total = size * nitems;
    auto *headers = static_cast<std::vector<std::pair<std::string, std::string>> *>(userdata);
    std::string_view line{buffer, total};
    if (line.ends_with("\r\n")) {
        line.remove_suffix(2);
    } else if (line.ends_with("\n")) {
        line.remove_suffix(1);
    }
    const auto colon = line.find(':');
    if (colon == std::string_view::npos) {
        return total;
    }
    auto name = std::string{line.substr(0, colon)};
    auto value = std::string{line.substr(colon + 1)};
    while (!value.empty() && value.front() == ' ') {
        value.erase(value.begin());
    }
    headers->emplace_back(std::move(name), std::move(value));
    return total;
}

}  // namespace

auto HttpClient::perform(CurlEasy &easy) -> HttpResponse {
    HttpResponse response;
    std::string buffer;
    CURL *const curl = easy.get();

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.response_headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    const auto code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        response.error = curl_easy_strerror(code);
        return response;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
    char *effective_url = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    if (effective_url != nullptr) {
        response.effective_url = effective_url;
    }
    response.body = std::move(buffer);
    return response;
}

auto HttpClient::request(const HttpRequestOptions &options) const -> HttpResponse {
    CurlEasy easy;
    CurlHeaders header_list;
    const std::string url_str{options.url};
    const std::string body_str{options.body};

    CURL *const curl = easy.get();
    curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, options.follow_location ? 1L : 0L);

    if (options.method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body_str.size()));
    } else {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }

    for (const auto &[key, value] : options.headers) {
        header_list.append(key + ": " + value);
    }
    if (header_list.get() != nullptr) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list.get());
    }

    return perform(easy);
}

auto HttpClient::download(
    std::string_view url,
    const std::filesystem::path &output_path,
    std::span<const std::pair<std::string, std::string>> headers
) const -> HttpResponse {
    CurlEasy easy;
    CurlHeaders header_list;
    const std::string url_str{url};

    std::filesystem::create_directories(output_path.parent_path());
    auto out = std::ofstream{output_path, std::ios::binary};
    if (!out) {
        HttpResponse response;
        response.error = "failed to open output file: " + output_path.string();
        return response;
    }

    CURL *const curl = easy.get();
    curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *contents, std::size_t size, std::size_t nmemb, void *userp) -> std::size_t {
        const auto total = size * nmemb;
        static_cast<std::ofstream *>(userp)->write(contents, static_cast<std::streamsize>(total));
        return total;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);

    for (const auto &[key, value] : headers) {
        header_list.append(key + ": " + value);
    }
    if (header_list.get() != nullptr) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list.get());
    }

    HttpResponse response;
    const auto code = curl_easy_perform(curl);
    out.close();
    if (code != CURLE_OK) {
        response.error = curl_easy_strerror(code);
        std::filesystem::remove(output_path);
        return response;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
    char *effective_url = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    if (effective_url != nullptr) {
        response.effective_url = effective_url;
    }
    return response;
}

auto HttpClient::get(std::string_view url) const -> HttpResponse {
    CurlEasy easy;
    const std::string url_str{url};

    curl_easy_setopt(easy.get(), CURLOPT_URL, url_str.c_str());

    return perform(easy);
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

    return perform(easy);
}

auto HttpClient::post_multipart(
    std::string_view url,
    std::span<const HttpMultipartPart> parts
) const -> HttpResponse {
    CurlEasy easy;
    const std::string url_str{url};
    CURL *const curl = easy.get();

    curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());

    curl_mime *const mime = curl_mime_init(curl);
    if (mime == nullptr) {
        HttpResponse response;
        response.error = "curl_mime_init failed";
        return response;
    }

    struct MimeGuard {
        curl_mime *handle;
        ~MimeGuard() {
            curl_mime_free(handle);
        }
    } guard{mime};

    for (const auto &part : parts) {
        curl_mimepart *const mime_part = curl_mime_addpart(mime);
        curl_mime_name(mime_part, part.name.c_str());

        if (!part.file_path.empty()) {
            if (curl_mime_filedata(mime_part, part.file_path.c_str()) != CURLE_OK) {
                HttpResponse response;
                response.error = "curl_mime_filedata failed: " + part.file_path;
                return response;
            }
        } else {
            curl_mime_data(mime_part, part.value.c_str(), CURL_ZERO_TERMINATED);
        }
    }

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    return perform(easy);
}
