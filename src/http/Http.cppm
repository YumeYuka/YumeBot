export module http;

import std;
import http.curl.easy;
import http.response;

export struct HttpMultipartPart {
    std::string name;
    std::string value;
    std::string file_path;
};

export struct HttpRequestOptions {
    std::string url;
    std::string method{"GET"};
    std::string body{};
    std::span<const std::pair<std::string, std::string>> headers{};
    bool follow_location{true};
};

export class HttpClient {
public:
    HttpClient();

    [[nodiscard]] auto get(std::string_view url) const -> HttpResponse;

    [[nodiscard]] auto request(const HttpRequestOptions &options) const -> HttpResponse;

    [[nodiscard]] auto post(
        std::string_view url,
        std::string_view body,
        std::span<const std::pair<std::string, std::string>> headers = {}
    ) const -> HttpResponse;

    [[nodiscard]] auto post_multipart(
        std::string_view url,
        std::span<const HttpMultipartPart> parts
    ) const -> HttpResponse;

    [[nodiscard]] auto download(
        std::string_view url,
        const std::filesystem::path &output_path,
        std::span<const std::pair<std::string, std::string>> headers = {}
    ) const -> HttpResponse;

private:
    static auto perform(CurlEasy &easy) -> HttpResponse;
};
