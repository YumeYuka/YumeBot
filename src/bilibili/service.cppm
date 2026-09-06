export module bilibili.service;

import http;
import std;

export enum class BilibiliLoginStatus {
    Success,
    Expired,
    Timeout,
};

export struct BilibiliLoginQrCode {
    std::string login_url{};
    std::string qr_code_key{};
};

export struct BilibiliDownloadedVideo {
    std::string title{};
    std::string summary{};
    std::string source_url{};
    std::string file_path{};
    int duration_seconds{0};
};

export class BilibiliService {
public:
    BilibiliService();

    [[nodiscard]] auto extract_video_url(std::string_view text) const -> std::optional<std::string>;

    [[nodiscard]] auto create_login_qr_code() const -> BilibiliLoginQrCode;

    [[nodiscard]] auto wait_for_login(const std::string &qr_code_key) const -> BilibiliLoginStatus;

    [[nodiscard]] auto download_video(std::string_view source_url) const -> BilibiliDownloadedVideo;

    auto delete_downloaded_file(const std::filesystem::path &path) const -> void;

private:
    HttpClient http_;
    std::filesystem::path credential_path_{"data/bilibili-credentials.json"};
    std::filesystem::path download_directory_{"data/bilibili-downloads"};
};
