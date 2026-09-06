module netease.service;

import common.util;
import config;
import http;
import log;
import netease.crypto;
import std;
import telegram.json;

namespace {

constexpr std::string_view k_netease_origin = "https://music.163.com";
constexpr std::string_view k_user_agent =
    "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/59.0.3071.115 Mobile Safari/537.36";
constexpr std::string_view k_anonymous_token =
    "4ee5f776c9ed1e4d5f031b09e084c6cb333e43ee4a841afeebbef9bbf4b7e4152b51ff20ecb9e8ee9e89ab23044cf50d1609e4781e805e73a138419e5583bc7fd1e5933c52368d9127ba9ce4e2f233bf5a77ba40ea6045ae1fc612ead95d7b0e0edf70a74334194e1a190979f5fc12e9968c3666a981495b33a649814e309366";

constexpr std::string_view k_eapi_song_detail_path = "/api/v3/song/detail";
constexpr std::string_view k_eapi_song_detail_url = "https://music.163.com/eapi/v3/song/detail";
constexpr std::string_view k_eapi_song_url_path = "/api/song/enhance/player/url/v1";
constexpr std::string_view k_eapi_song_url_url = "https://music.163.com/eapi/song/enhance/player/url/v1";

const std::array<std::string_view, 4> k_quality_levels = {"hires", "lossless", "higher", "standard"};

auto append_cookie(std::string &cookie, std::string_view key, std::string_view value) -> void {
    if (!cookie.empty()) {
        cookie += "; ";
    }
    cookie += encode_url_component(key) + "=" + encode_url_component(value);
}

auto base64_encode(std::span<const std::byte> input) -> std::string {
    static constexpr char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    encoded.reserve(((input.size() + 2) / 3) * 4);
    std::size_t index = 0;
    while (index + 2 < input.size()) {
        const auto b0 = static_cast<unsigned char>(input[index++]);
        const auto b1 = static_cast<unsigned char>(input[index++]);
        const auto b2 = static_cast<unsigned char>(input[index++]);
        encoded += table[b0 >> 2];
        encoded += table[((b0 & 0x03) << 4) | (b1 >> 4)];
        encoded += table[((b1 & 0x0F) << 2) | (b2 >> 6)];
        encoded += table[b2 & 0x3F];
    }
    if (index < input.size()) {
        const auto b0 = static_cast<unsigned char>(input[index++]);
        encoded += table[b0 >> 2];
        if (index < input.size()) {
            const auto b1 = static_cast<unsigned char>(input[index++]);
            encoded += table[((b0 & 0x03) << 4) | (b1 >> 4)];
            encoded += table[(b1 & 0x0F) << 2];
            encoded += '=';
        } else {
            encoded += table[(b0 & 0x03) << 4];
            encoded += "==";
        }
    }
    return encoded;
}

auto netease_pic_key(std::string_view pic_id) -> std::string {
    static constexpr std::string_view magic = "3go8&$8*3*3h0k(2)2";
    std::string transformed;
    transformed.reserve(pic_id.size());
    for (std::size_t i = 0; i < pic_id.size(); ++i) {
        transformed += static_cast<char>(
            static_cast<unsigned char>(pic_id[i]) ^ static_cast<unsigned char>(magic[i % magic.size()])
        );
    }
    const auto digest = md5_digest(std::as_bytes(std::span<const char>{transformed.data(), transformed.size()}));
    auto encoded = base64_encode(digest);
    std::ranges::replace(encoded, '/', '_');
    std::ranges::replace(encoded, '+', '-');
    return encoded;
}

auto json_escape_for_string(std::string_view text) -> std::string {
    std::string escaped;
    escaped.reserve(text.size() + 8);
    for (const char ch : text) {
        if (ch == '"') {
            escaped += "\\\"";
        } else if (ch == '\\') {
            escaped += "\\\\";
        } else {
            escaped += ch;
        }
    }
    return escaped;
}

auto build_song_detail_payload(const std::string &song_id) -> std::string {
    const auto items_json = std::string{"[{\"id\":"} + song_id + "}]";
    return std::string{"{\"c\":\""} + json_escape_for_string(items_json) + "\"}";
}

auto build_song_url_payload(const std::string &song_id, std::string_view level) -> std::string {
    const auto ids_json = std::string{"[\""} + song_id + "\"]";
    return std::string{"{\"ids\":\""} + json_escape_for_string(ids_json)
        + "\",\"encodeType\":\"mp3\",\"level\":\"" + std::string{level} + "\"}";
}

auto with_cover_param(std::string_view url, int size = 300) -> std::string {
    if (url.empty()) {
        return "";
    }
    const auto param = "param=" + std::to_string(size) + "y" + std::to_string(size);
    return url.contains('?') ? std::string{url} + "&" + param : std::string{url} + "?" + param;
}

auto album_cover_url(const JsonValue &album) -> std::string {
    auto cover = json_string(album, "picUrl").value_or("");
    if (cover.empty()) {
        auto pic_id = json_string(album, "pic_str").value_or("");
        if (pic_id.empty()) {
            const auto pic = json_i64(album, "pic").value_or(0);
            if (pic > 0) {
                pic_id = std::to_string(pic);
            }
        }
        if (!pic_id.empty() && pic_id != "0") {
            cover = "https://p3.music.126.net/" + netease_pic_key(pic_id) + "/" + pic_id + ".jpg";
        }
    }
    if (cover.starts_with("http://")) {
        cover.replace(0, 4, "https");
    }
    return cover;
}

struct SongDetail {
    std::string name{};
    std::string artists{};
    std::string album{};
    std::string cover_url{};
    int duration_seconds{0};
};

struct PlayableStream {
    std::string url{};
    std::int64_t size_bytes{0};
    int bitrate_kbps{0};
    std::string level{};
    std::string format{};
};

auto default_headers() -> std::array<std::pair<std::string, std::string>, 2> {
    return {
        std::pair<std::string, std::string>{"User-Agent", std::string{k_user_agent}},
        std::pair<std::string, std::string>{"Referer", std::string{k_netease_origin}},
    };
}

auto random_mainland_ip() -> std::string {
    static const std::array<std::pair<int, int>, 14> prefixes = {{
        {113, 0}, {113, 64}, {113, 128}, {114, 214}, {118, 122}, {119, 112}, {211, 161},
        {221, 238}, {116, 224}, {222, 128}, {183, 128}, {116, 128}, {101, 226}, {61, 128},
    }};
    static thread_local std::mt19937 rng{std::random_device{}()};
    const auto &[a, b] = prefixes[rng() % prefixes.size()];
    std::uniform_int_distribution<int> dist(1, 254);
    return std::to_string(a) + "." + std::to_string(b) + "." + std::to_string(dist(rng)) + "." + std::to_string(dist(rng));
}

auto random_nmtid() -> std::string {
    static constexpr std::string_view alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(0, alphabet.size() - 1);
    std::string out = "00";
    for (int i = 0; i < 30; ++i) {
        out += alphabet[dist(rng)];
    }
    return out;
}

auto extract_music_u(std::string_view raw) -> std::string {
    auto value = std::string{raw};
    while (!value.empty() && (value.front() == '`' || value.front() == '"' || value.front() == '\'')) {
        value.erase(value.begin());
    }
    while (!value.empty() && (value.back() == '`' || value.back() == '"' || value.back() == '\'')) {
        value.pop_back();
    }
    if (const auto pos = value.find("MUSIC_U="); pos != std::string::npos) {
        value = value.substr(pos + 8);
        if (const auto end = value.find(';'); end != std::string::npos) {
            value = value.substr(0, end);
        }
    }
    return value;
}

auto build_cookie(const std::optional<std::string> &music_u) -> std::string {
    const auto buildver = std::to_string(now_millis() / 1000).substr(0, 10);
    std::string cookie;
    append_cookie(cookie, "appver", "8.9.70");
    append_cookie(cookie, "buildver", buildver);
    append_cookie(cookie, "resolution", "1920x1080");
    append_cookie(cookie, "os", "android");
    append_cookie(cookie, "NMTID", random_nmtid());
    const auto account = music_u.has_value() ? extract_music_u(*music_u) : std::string{};
    if (!account.empty()) {
        append_cookie(cookie, "MUSIC_U", account);
    } else {
        append_cookie(cookie, "MUSIC_A", std::string{k_anonymous_token});
    }
    return cookie;
}

auto is_url_char(unsigned char ch) -> bool {
    return std::isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == '/'
        || ch == ':' || ch == '?' || ch == '=' || ch == '&' || ch == '%' || ch == '#';
}

auto trim_url_token(std::string_view url) -> std::string {
    std::size_t begin = 0;
    std::size_t end = url.size();
    while (begin < end && !is_url_char(static_cast<unsigned char>(url[begin]))) {
        ++begin;
    }
    while (end > begin && !is_url_char(static_cast<unsigned char>(url[end - 1]))) {
        --end;
    }
    return std::string{url.substr(begin, end - begin)};
}

auto ensure_http_scheme(std::string url) -> std::string {
    if (url.starts_with("http://") || url.starts_with("https://")) {
        return url;
    }
    if (url.starts_with("163cn.") || url.contains("music.163.com")) {
        return "https://" + url;
    }
    return url;
}

auto extract_song_id_from_text(std::string_view text) -> std::optional<std::string> {
    static const std::regex id_query{R"([?&#]id=(\d{5,20}))", std::regex::icase};
    static const std::regex song_path{R"(/song/?(\d{5,20}))", std::regex::icase};
    static const std::regex song_id_field{R"("songId"\s*:\s*(\d{5,20}))"};
    std::match_results<std::string_view::const_iterator> match;
    if (std::regex_search(text.begin(), text.end(), match, id_query)) {
        const auto song_id = match[1].str();
        if (song_id != "0") {
            return song_id;
        }
    }
    if (std::regex_search(text.begin(), text.end(), match, song_path)) {
        const auto song_id = match[1].str();
        if (song_id != "0") {
            return song_id;
        }
    }
    if (std::regex_search(text.begin(), text.end(), match, song_id_field)) {
        return match[1].str();
    }
    return std::nullopt;
}

auto extract_song_id(std::string_view url) -> std::optional<std::string> {
    if (const auto song_id = extract_song_id_from_text(url)) {
        return song_id;
    }
    if (!url.contains("music.163.com")) {
        return std::nullopt;
    }
    static const std::regex song_path{R"(/song/?(\d{5,20}))"};
    const auto path_part = url.substr(url.find("music.163.com"));
    std::match_results<std::string_view::const_iterator> match;
    if (std::regex_search(path_part.begin(), path_part.end(), match, song_path)) {
        const auto song_id = match[1].str();
        if (song_id != "0") {
            return song_id;
        }
    }
    return std::nullopt;
}

auto expand_short_url(const HttpClient &http, std::string_view source_url) -> std::string {
    auto url = ensure_http_scheme(trim_url_token(source_url));
    if (!url.contains("163cn.")) {
        return url;
    }

    const auto response = http.request(HttpRequestOptions{
        .url = url,
        .headers = default_headers(),
    });
    if (!response.effective_url.empty() && response.effective_url.contains("music.163.com")) {
        if (const auto song_id = extract_song_id(response.effective_url)) {
            return std::string{k_netease_origin} + "/song?id=" + *song_id;
        }
        return response.effective_url;
    }
    if (const auto song_id = extract_song_id_from_text(response.body)) {
        return std::string{k_netease_origin} + "/song?id=" + *song_id;
    }
    if (!response.effective_url.empty()) {
        if (const auto song_id = extract_song_id_from_text(response.effective_url)) {
            return std::string{k_netease_origin} + "/song?id=" + *song_id;
        }
        return response.effective_url;
    }
    return url;
}

auto rewrite_netease_cdn_url(std::string url) -> std::string {
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 4> replacements = {{
        {"m8.", "m7."},
        {"m801.", "m701."},
        {"m804.", "m701."},
        {"m704.", "m701."},
    }};
    for (const auto &[from, to] : replacements) {
        std::size_t pos = 0;
        while ((pos = url.find(from, pos)) != std::string::npos) {
            url.replace(pos, from.size(), to);
            pos += to.size();
        }
    }
    return url;
}

auto download_with_integrity(
    const HttpClient &http,
    std::string url,
    const std::filesystem::path &output_path,
    std::int64_t expected_size,
    std::span<const std::pair<std::string, std::string>> headers
) -> void {
    url = rewrite_netease_cdn_url(std::move(url));
    constexpr int max_attempts = 3;
    std::string last_error = "网易云音频下载失败";
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        const auto result = http.download(url, output_path, headers);
        if (!result.ok()) {
            last_error = result.error.empty() ? "网易云音频下载失败" : result.error;
            std::error_code ec;
            std::filesystem::remove(output_path, ec);
            continue;
        }

        std::error_code ec;
        const auto actual = std::filesystem::file_size(output_path, ec);
        if (ec || actual == 0) {
            last_error = "网易云音频下载结果为空";
            std::filesystem::remove(output_path, ec);
            continue;
        }
        if (expected_size > 0 && static_cast<std::int64_t>(actual) != expected_size) {
            last_error = "音频下载不完整：实际 " + std::to_string(actual) + " 字节，期望 "
                + std::to_string(expected_size) + " 字节";
            std::filesystem::remove(output_path, ec);
            continue;
        }
        return;
    }
    throw std::runtime_error(last_error);
}

auto parse_eapi_json(std::string_view body, std::string_view sign_path) -> JsonValue {
    try {
        return JsonValue::parse(body);
    } catch (const std::exception &) {
        const auto preview = body.size() > 160 ? std::string{body.substr(0, 160)} + "..." : std::string{body};
        throw std::runtime_error(
            "网易云接口返回了无法解析的 JSON（" + std::string{sign_path} + "）：" + preview
        );
    }
}

auto eapi_post(
    const HttpClient &http,
    std::string_view sign_path,
    std::string_view endpoint_url,
    std::string_view json_payload,
    const std::optional<std::string> &music_u
) -> JsonValue {
    const auto body = eapi_params(sign_path, json_payload);
    const auto spoofed_ip = random_mainland_ip();
    const std::array headers = {
        std::pair<std::string, std::string>{"User-Agent", std::string{k_user_agent}},
        std::pair<std::string, std::string>{"Cookie", build_cookie(music_u)},
        std::pair<std::string, std::string>{"X-Real-IP", spoofed_ip},
        std::pair<std::string, std::string>{"X-Forwarded-For", spoofed_ip},
        std::pair<std::string, std::string>{"HTTP_X_FORWARDED_FOR", spoofed_ip},
        std::pair<std::string, std::string>{"CLIENT-IP", spoofed_ip},
        std::pair<std::string, std::string>{"Content-Type", "application/x-www-form-urlencoded"},
    };
    const auto response = http.post(std::string{endpoint_url}, body, headers);
    if (!response.ok()) {
        throw std::runtime_error(response.error.empty() ? "网易云接口请求失败" : response.error);
    }
    const auto json = parse_eapi_json(response.body, sign_path);
    if (json_i64(json, "code").value_or(0) != 200) {
        const auto code = json_i64(json, "code").value_or(0);
        const auto message = json_string(json, "message").value_or(json_string(json, "msg").value_or(""));
        std::string error = "网易云接口请求失败（" + std::string{sign_path} + "，错误码 " + std::to_string(code);
        if (!message.empty()) {
            error += "，" + message;
        }
        error += "）";
        throw std::runtime_error(error);
    }
    return json;
}

auto fetch_song_detail(
    const HttpClient &http,
    const std::string &song_id,
    const std::optional<std::string> &music_u
) -> SongDetail {
    const auto payload = build_song_detail_payload(song_id);
    const auto json = eapi_post(
        http,
        k_eapi_song_detail_path,
        k_eapi_song_detail_url,
        payload,
        music_u
    );
    const auto *songs = json_array(json, "songs");
    if (songs == nullptr || songs->empty()) {
        throw std::runtime_error("歌曲不存在或已下架。");
    }
    const auto &song = songs->front();
    SongDetail detail;
    detail.name = json_string(song, "name").value_or("未知歌曲");
    if (const auto *artists = json_array(song, "ar")) {
        for (const auto &artist : *artists) {
            if (!detail.artists.empty()) {
                detail.artists += " / ";
            }
            detail.artists += json_string(artist, "name").value_or("");
        }
    }
    if (detail.artists.empty()) {
        detail.artists = "未知歌手";
    }
    if (const auto *album = json_object(song, "al")) {
        detail.album = json_string(*album, "name").value_or("");
        detail.cover_url = album_cover_url(*album);
    }
    detail.duration_seconds = static_cast<int>(json_i64(song, "dt").value_or(0) / 1000);
    return detail;
}

auto resolve_playable_stream(
    const HttpClient &http,
    const std::string &song_id,
    const std::optional<std::string> &music_u,
    std::int64_t max_audio_bytes
) -> PlayableStream {
    std::string last_failure = "歌曲无版权或已下架。";
    for (const auto level : k_quality_levels) {
        const auto payload = build_song_url_payload(song_id, level);
        const auto json = eapi_post(
            http,
            k_eapi_song_url_path,
            k_eapi_song_url_url,
            payload,
            music_u
        );
        const auto *items = json_array(json, "data");
        if (items == nullptr || items->empty()) {
            continue;
        }
        const auto &item = items->front();
        if (json_i64(item, "code").value_or(0) != 200) {
            last_failure = "歌曲无版权、需 VIP 或当前音质不可用。";
            continue;
        }
        const auto url = json_string(item, "url").value_or("");
        if (url.empty()) {
            continue;
        }
        const auto size_bytes = json_i64(item, "size").value_or(0);
        if (size_bytes > max_audio_bytes) {
            last_failure = "音频文件超过 Telegram 上传限制。";
            continue;
        }
        PlayableStream stream;
        stream.url = url;
        stream.size_bytes = size_bytes;
        stream.bitrate_kbps = static_cast<int>(json_i64(item, "br").value_or(0) / 1000);
        stream.level = json_string(item, "level").value_or(std::string{level});
        stream.format = json_string(item, "type").value_or("mp3");
        std::ranges::transform(stream.format, stream.format.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return stream;
    }
    throw std::runtime_error(last_failure);
}

}  // namespace

NeteaseService::NeteaseService(std::optional<std::string> music_u, bool local_bot_api_server)
    : music_u_{std::move(music_u)}
    , max_audio_bytes_{max_audio_bytes(local_bot_api_server)} {}

auto NeteaseService::max_audio_bytes(bool local_bot_api_server) -> std::int64_t {
    return local_bot_api_server ? 1900LL * 1024 * 1024 : 48LL * 1024 * 1024;
}

auto NeteaseService::extract_song_url(std::string_view text) const -> std::optional<std::string> {
    static const std::regex short_link{
        R"((?:https?://)?(?:[a-z0-9-]+\.)?163cn\.(?:tv|link)/[A-Za-z0-9]+)",
        std::regex::icase,
    };
    static const std::regex full_link{
        R"((?:https?://)?(?:[a-z0-9-]+\.)?music\.163\.com/[^\s<>()，。；、]+)",
        std::regex::icase,
    };
    std::match_results<std::string_view::const_iterator> match;
    if (std::regex_search(text.begin(), text.end(), match, short_link)) {
        return ensure_http_scheme(trim_url_token(match[0].str()));
    }
    if (std::regex_search(text.begin(), text.end(), match, full_link)) {
        return ensure_http_scheme(trim_url_token(match[0].str()));
    }
    return std::nullopt;
}

auto NeteaseService::delete_downloaded_file(const std::filesystem::path &path) const -> void {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

auto NeteaseService::download_song(std::string_view source_url) const -> NeteaseDownloadedSong {
    const auto page_url = expand_short_url(http_, source_url);

    const auto song_id = extract_song_id(page_url);
    if (!song_id.has_value()) {
        throw std::runtime_error("无法识别网易云歌曲链接（仅支持单曲，暂不支持歌单/专辑）。");
    }

    const auto detail = fetch_song_detail(http_, *song_id, music_u_);
    const auto playable = resolve_playable_stream(http_, *song_id, music_u_, max_audio_bytes_);

    const auto file_name = sanitize_file_name(detail.artists + " - " + detail.name, "netease-song");
    const auto output_path = download_directory_ / (file_name + "." + playable.format);
    download_with_integrity(http_, playable.url, output_path, playable.size_bytes, default_headers());

    std::optional<std::string> thumbnail_path;
    if (!detail.cover_url.empty()) {
        const auto thumb_url = with_cover_param(detail.cover_url);
        const auto thumb_file = download_directory_ / (file_name + "-thumbnail.jpg");
        const auto thumb_download = http_.download(thumb_url, thumb_file, default_headers());
        if (thumb_download.ok()) {
            thumbnail_path = thumb_file.string();
        }
    }

    return NeteaseDownloadedSong{
        .name = detail.name,
        .artists = detail.artists,
        .album = detail.album,
        .thumbnail_path = thumbnail_path,
        .track_url = std::string{k_netease_origin} + "/song?id=" + *song_id,
        .file_path = output_path.string(),
        .duration_seconds = detail.duration_seconds,
        .size_bytes = playable.size_bytes,
        .bitrate_kbps = playable.bitrate_kbps,
        .level = playable.level,
        .format = playable.format,
    };
}
