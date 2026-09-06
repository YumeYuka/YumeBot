export module netease.service;

import config;
import http;
import std;

export struct NeteaseDownloadedSong {
    std::string name{};
    std::string artists{};
    std::string album{};
    std::optional<std::string> thumbnail_path{};
    std::string track_url{};
    std::string file_path{};
    int duration_seconds{0};
    std::int64_t size_bytes{0};
    int bitrate_kbps{0};
    std::string level{};
    std::string format{};
};

export class NeteaseService {
public:
    explicit NeteaseService(std::optional<std::string> music_u, bool local_bot_api_server);

    [[nodiscard]] auto extract_song_url(std::string_view text) const -> std::optional<std::string>;

    [[nodiscard]] auto download_song(std::string_view source_url) const -> NeteaseDownloadedSong;

    auto delete_downloaded_file(const std::filesystem::path &path) const -> void;

    static auto max_audio_bytes(bool local_bot_api_server) -> std::int64_t;

private:
    HttpClient http_;
    std::optional<std::string> music_u_;
    std::int64_t max_audio_bytes_{};
    std::filesystem::path download_directory_{"data/netease-downloads"};
};
