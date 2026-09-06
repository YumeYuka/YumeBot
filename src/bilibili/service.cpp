module bilibili.service;

import common.util;
import http;
import log;
import std;
import telegram.json;

namespace {
    constexpr std::string_view k_bilibili_origin = "https://www.bilibili.com";
    constexpr std::string_view k_user_agent =
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
    constexpr int k_max_video_duration_seconds = 10 * 60;
    constexpr int k_login_poll_delay_ms = 2000;
    constexpr int k_login_timeout_ms = 180000;
    constexpr int k_max_quality = 120;
    constexpr int k_dash_fnval = 4048;

    struct BilibiliCredentials {
        std::string sessdata{};
        std::string bili_jct{};
        std::string dede_user_id{};
        std::string dede_user_id_ck_md5{};

        [[nodiscard]] auto as_cookie_header() const -> std::string {
            std::string cookie;
            auto append = [&](std::string_view name, const std::string &value) {
                if (value.empty()) {
                    return;
                }
                if (!cookie.empty()) {
                    cookie += "; ";
                }
                cookie += std::string{name} + "=" + value;
            };
            append("SESSDATA", sessdata);
            append("bili_jct", bili_jct);
            append("DedeUserID", dede_user_id);
            append("DedeUserID__ckMd5", dede_user_id_ck_md5);
            return cookie;
        }
    };

    struct BilibiliPage {
        std::int64_t cid{};
        std::string title{};
        int duration_seconds{0};
    };

    struct VideoStreams {
        std::string video_url{};
        std::optional<std::string> audio_url{};
    };

    auto bilibili_headers(const std::string &cookie = {}) -> std::vector<std::pair<std::string, std::string> > {
        std::vector<std::pair<std::string, std::string> > headers = {
            {"User-Agent", std::string{k_user_agent}},
            {"Referer", std::string{k_bilibili_origin}},
            {"Origin", std::string{k_bilibili_origin}},
        };
        if (!cookie.empty()) {
            headers.emplace_back("Cookie", cookie);
        }
        return headers;
    }

    auto load_credentials(const std::filesystem::path &path) -> BilibiliCredentials {
        if (!std::filesystem::exists(path)) {
            return {};
        }
        std::ifstream in{path};
        const std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
        const auto json = JsonValue::parse(text);
        return BilibiliCredentials{
            .sessdata = json_string(json, "SESSDATA").value_or(""),
            .bili_jct = json_string(json, "bili_jct").value_or(""),
            .dede_user_id = json_string(json, "DedeUserID").value_or(""),
            .dede_user_id_ck_md5 = json_string(json, "DedeUserID__ckMd5").value_or(""),
        };
    }

    auto save_credentials(const std::filesystem::path &path, const BilibiliCredentials &credentials) -> void {
        std::filesystem::create_directories(path.parent_path());
        JsonValue::Object object;
        json_put(object, "SESSDATA", credentials.sessdata);
        json_put(object, "bili_jct", credentials.bili_jct);
        json_put(object, "DedeUserID", credentials.dede_user_id);
        json_put(object, "DedeUserID__ckMd5", credentials.dede_user_id_ck_md5);
        std::ofstream out{path};
        out << JsonValue::object(std::move(object)).dump();
    }

    auto parse_response_data(const std::string &body, std::string_view operation) -> JsonValue {
        const auto json = JsonValue::parse(body);
        const auto code = json_i64(json, "code").value_or(-1);
        if (code != 0) {
            const auto message = json_string(json, "message").value_or("未知错误");
            throw std::runtime_error(
                std::string{operation} + "失败（" + std::to_string(code) + "）：" + message
            );
        }
        if (const auto *data = json.get("data")) {
            return *data;
        }
        throw std::runtime_error(std::string{operation} + "响应缺少 data。");
    }

    auto normalize_bvid(std::string_view bvid) -> std::string {
        auto value = std::string{bvid};
        if (value.size() >= 2 && (value[0] == 'B' || value[0] == 'b') && (value[1] == 'V' || value[1] == 'v')) {
            value[0] = 'B';
            value[1] = 'V';
        }
        return value;
    }

    auto video_target_params(std::string_view url) -> std::vector<std::pair<std::string, std::string> > {
        static const std::regex bvid_pattern{R"(BV[0-9A-Za-z]{10})", std::regex::icase};
        static const std::regex avid_pattern{R"(av(\d+))", std::regex::icase};
        std::match_results<std::string_view::const_iterator> match;
        if (std::regex_search(url.begin(), url.end(), match, bvid_pattern)) {
            return {{"bvid", normalize_bvid(match[0].str())}};
        }
        if (std::regex_search(url.begin(), url.end(), match, avid_pattern)) {
            return {{"aid", match[1].str()}};
        }
        throw std::runtime_error("无法识别 B 站视频链接。");
    }

    auto append_query(std::string &url, const std::vector<std::pair<std::string, std::string> > &params) -> void {
        for (const auto &[key, value]: params) {
            url += url.contains('?') ? '&' : '?';
            url += key + '=' + value;
        }
    }

    auto get_json(
        const HttpClient &http,
        std::string url,
        const std::vector<std::pair<std::string, std::string> > &params,
        const std::string &cookie
    ) -> JsonValue {
        append_query(url, params);
        const auto response = http.request(HttpRequestOptions{
            .url = url,
            .headers = bilibili_headers(cookie),
        });
        if (!response.ok()) {
            throw std::runtime_error(response.error.empty() ? "B站接口请求失败" : response.error);
        }
        return parse_response_data(response.body, "B站接口请求");
    }

    auto run_command(std::string_view command) -> void {
        if (std::system(std::string{command}.c_str()) != 0) {
            throw std::runtime_error("媒体处理失败。");
        }
    }

    auto extract_cookie(
        const std::vector<std::pair<std::string, std::string> > &headers,
        std::string_view name
    ) -> std::string {
        const auto pattern = std::regex{std::string{"^"} + std::string{name} + "=([^;]+)"};
        for (const auto &[key, value]: headers) {
            if (key != "Set-Cookie" && key != "set-cookie") {
                continue;
            }
            if (std::match_results<std::string::const_iterator> match;
                std::regex_search(value.begin(), value.end(), match, pattern)) {
                return match[1].str();
            }
        }
        return {};
    }
} // namespace

BilibiliService::BilibiliService() = default;

auto BilibiliService::extract_video_url(std::string_view text) const -> std::optional<std::string> {
    static const std::regex b23{R"(https?://b23\.tv/[^\s<>()]+)", std::regex::icase};
    static const std::regex video{R"(https?://(?:www\.)?bilibili\.com/video/[^\s<>()]+)", std::regex::icase};
    static const std::regex bvid{R"(BV[0-9A-Za-z]{10})", std::regex::icase};
    static const std::regex avid{R"(av\d+)", std::regex::icase};
    std::match_results<std::string_view::const_iterator> match;
    if (std::regex_search(text.begin(), text.end(), match, b23)) {
        return match[0].str();
    }
    if (std::regex_search(text.begin(), text.end(), match, video)) {
        return match[0].str();
    }
    if (std::regex_search(text.begin(), text.end(), match, bvid)) {
        return std::string{k_bilibili_origin} + "/video/" + normalize_bvid(match[0].str());
    }
    if (std::regex_search(text.begin(), text.end(), match, avid)) {
        return std::string{k_bilibili_origin} + "/video/" + match[0].str();
    }
    return std::nullopt;
}

auto BilibiliService::create_login_qr_code() const -> BilibiliLoginQrCode {
    const auto data = get_json(
        http_,
        "https://passport.bilibili.com/x/passport-login/web/qrcode/generate",
        {},
        {}
    );
    BilibiliLoginQrCode qr;
    qr.login_url = json_string(data, "url").value_or("");
    qr.qr_code_key = json_string(data, "qrcode_key").value_or("");
    if (qr.login_url.empty() || qr.qr_code_key.empty()) {
        throw std::runtime_error("B站二维码生成响应不完整。");
    }
    return qr;
}

auto BilibiliService::wait_for_login(const std::string &qr_code_key) const -> BilibiliLoginStatus {
    const auto deadline = now_millis() + k_login_timeout_ms;
    while (now_millis() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{k_login_poll_delay_ms});
        const auto response = http_.request(HttpRequestOptions{
            .url = "https://passport.bilibili.com/x/passport-login/web/qrcode/poll?qrcode_key="
                   + encode_url_component(qr_code_key),
            .headers = bilibili_headers(),
        });
        if (!response.ok()) {
            continue;
        }

        const auto json = JsonValue::parse(response.body);
        if (json_i64(json, "code").value_or(-1) != 0) {
            continue;
        }
        const auto *data = json.get("data");
        if (data == nullptr) {
            continue;
        }

        const auto poll_code = json_i64(*data, "code").value_or(-1);
        if (poll_code == 86090 || poll_code == 86101) {
            continue;
        }
        if (poll_code == 86038) {
            return BilibiliLoginStatus::Expired;
        }
        if (poll_code != 0) {
            Log::Warn("Bilibili login poll returned unexpected status: {}", poll_code);
            continue;
        }

        BilibiliCredentials credentials{
            .sessdata = extract_cookie(response.response_headers, "SESSDATA"),
            .bili_jct = extract_cookie(response.response_headers, "bili_jct"),
            .dede_user_id = extract_cookie(response.response_headers, "DedeUserID"),
            .dede_user_id_ck_md5 = extract_cookie(response.response_headers, "DedeUserID__ckMd5"),
        };
        if (credentials.bili_jct.empty()) {
            credentials.bili_jct = json_string(*data, "bili_jct").value_or("");
        }
        if (credentials.as_cookie_header().empty()) {
            throw std::runtime_error("B站登录未返回有效 Cookie。");
        }
        save_credentials(credential_path_, credentials);
        return BilibiliLoginStatus::Success;
    }
    return BilibiliLoginStatus::Timeout;
}

auto BilibiliService::delete_downloaded_file(const std::filesystem::path &path) const -> void {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

auto BilibiliService::download_video(std::string_view source_url) const -> BilibiliDownloadedVideo {
    auto page_url = std::string{source_url};
    if (page_url.contains("b23.tv")) {
        const auto response = http_.request(HttpRequestOptions{
            .url = page_url,
            .headers = bilibili_headers(),
        });
        if (!response.effective_url.empty()) {
            page_url = response.effective_url;
        }
    }

    const auto target_params = video_target_params(page_url);
    const auto cookie = load_credentials(credential_path_).as_cookie_header();

    const auto pages_data = get_json(
        http_,
        "https://api.bilibili.com/x/player/pagelist",
        target_params,
        cookie
    );
    const auto *pages = pages_data.as_array();
    if (pages == nullptr || pages->empty()) {
        throw std::runtime_error("无法获取 B 站分P信息。");
    }
    int page_number = 1;
    static const std::regex page_param{R"([?&]p=(\d+))"};
    std::match_results<std::string::const_iterator> page_match;
    if (std::regex_search(page_url, page_match, page_param)) {
        page_number = std::stoi(page_match[1].str());
    }
    if (page_number < 1 || static_cast<std::size_t>(page_number) > pages->size()) {
        throw std::runtime_error("请求的分P不存在。");
    }
    const auto &page_json = (*pages)[static_cast<std::size_t>(page_number - 1)];
    BilibiliPage page{
        .cid = json_i64(page_json, "cid").value_or(0),
        .title = json_string(page_json, "part").value_or("B站视频"),
        .duration_seconds = static_cast<int>(json_i64(page_json, "duration").value_or(0)),
    };

    std::string title = page.title;
    std::string summary;
    int duration_seconds = page.duration_seconds;
    try {
        const auto view = get_json(
            http_,
            "https://api.bilibili.com/x/web-interface/view",
            target_params,
            cookie
        );
        title = json_string(view, "title").value_or(title);
        summary = json_string(view, "desc").value_or("");
        duration_seconds = static_cast<int>(json_i64(view, "duration").value_or(duration_seconds));
    } catch (const std::exception &) {
        Log::Warn("Bilibili view metadata unavailable, using pagelist fallback");
    }

    if (duration_seconds <= 0 || duration_seconds > k_max_video_duration_seconds) {
        throw std::runtime_error("视频时长超过 10 分钟，未发送。");
    }

    auto play_params = target_params;
    play_params.emplace_back("cid", std::to_string(page.cid));
    play_params.emplace_back("qn", std::to_string(k_max_quality));
    play_params.emplace_back("fnver", "0");
    play_params.emplace_back("fnval", "0");
    play_params.emplace_back("fourk", "1");
    play_params.emplace_back("otype", "json");
    play_params.emplace_back("platform", "html5");
    play_params.emplace_back("high_quality", "1");

    const auto merged = get_json(http_, "https://api.bilibili.com/x/player/playurl", play_params, cookie);
    VideoStreams streams;
    if (const auto *durl = json_array(merged, "durl"); durl != nullptr && !durl->empty()) {
        streams.video_url = json_string(durl->front(), "url").value_or("");
    } else {
        play_params.back().second = std::to_string(k_dash_fnval);
        const auto dash_root = get_json(http_, "https://api.bilibili.com/x/player/playurl", play_params, cookie);
        const auto *dash = json_object(dash_root, "dash");
        if (dash == nullptr) {
            throw std::runtime_error("B站未返回可下载的媒体流。");
        }
        if (const auto *videos = json_array(*dash, "video"); videos != nullptr && !videos->empty()) {
            streams.video_url = json_string(videos->back(), "baseUrl").value_or(
                json_string(videos->back(), "base_url").value_or("")
            );
        }
        if (const auto *audios = json_array(*dash, "audio"); audios != nullptr && !audios->empty()) {
            streams.audio_url = json_string(audios->back(), "baseUrl").value_or(
                json_string(audios->back(), "base_url").value_or("")
            );
        }
    }
    if (streams.video_url.empty()) {
        throw std::runtime_error("B站未返回可下载的视频流。");
    }

    std::filesystem::create_directories(download_directory_);
    const auto output_path = download_directory_ / (sanitize_file_name(title, "bilibili-video") + ".mp4");
    const auto video_temp = output_path;
    const auto video_part = download_directory_ / (output_path.stem().string() + ".video.m4s");
    const auto audio_part = download_directory_ / (output_path.stem().string() + ".audio.m4s");

    const auto media_headers = bilibili_headers(cookie);
    const auto video_download = http_.download(streams.video_url, video_part, media_headers);
    if (!video_download.ok()) {
        throw std::runtime_error("B站视频流下载失败");
    }

    if (!streams.audio_url.has_value()) {
        run_command("ffmpeg -nostdin -y -loglevel error -i " + shell_quote(video_part.string())
                    + " -map 0:v:0 -map 0:a:0? -c copy -movflags +faststart " + shell_quote(output_path.string()));
        delete_downloaded_file(video_part);
        return BilibiliDownloadedVideo{
            .title = title,
            .summary = summary,
            .source_url = page_url,
            .file_path = output_path.string(),
            .duration_seconds = duration_seconds,
        };
    }

    const auto audio_download = http_.download(*streams.audio_url, audio_part, media_headers);
    if (!audio_download.ok()) {
        throw std::runtime_error("B站音频流下载失败");
    }
    run_command("ffmpeg -nostdin -y -loglevel error -i " + shell_quote(video_part.string())
                + " -i " + shell_quote(audio_part.string())
                + " -map 0:v:0 -map 1:a:0 -c:v copy -c:a aac -b:a 192k -shortest -movflags +faststart "
                + shell_quote(output_path.string()));
    delete_downloaded_file(video_part);
    delete_downloaded_file(audio_part);

    return BilibiliDownloadedVideo{
        .title = title,
        .summary = summary,
        .source_url = page_url,
        .file_path = output_path.string(),
        .duration_seconds = duration_seconds,
    };
}
