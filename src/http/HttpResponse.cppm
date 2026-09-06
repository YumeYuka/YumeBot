export module http.response;

import std;

export struct HttpResponse {
    long status_code{0};
    std::string body{};
    std::string effective_url{};
    std::vector<std::pair<std::string, std::string>> response_headers{};
    std::string error{};  // curl 层错误，空表示 perform 成功

    [[nodiscard]] auto ok() const noexcept -> bool {
        return error.empty() && status_code >= 200 && status_code < 300;
    }
};