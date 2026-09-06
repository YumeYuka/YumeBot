module netease.handler;

import common.util;
import log;
import netease.service;
import std;
import telegram;
import telegram.message;
import telegram.request;

namespace {

auto format_file_size(std::int64_t bytes) -> std::string {
    if (bytes >= 1024 * 1024) {
        const auto tenths = (bytes * 10) / (1024 * 1024);
        return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "MB";
    }
    if (bytes > 0) {
        return std::to_string(bytes / 1024) + "KB";
    }
    return "大小未知";
}

auto format_song_caption(const NeteaseDownloadedSong &song, const std::string &bot_username) -> std::string {
    const auto name_link = "<a href=\"" + escape_html_attribute(song.track_url) + "\">"
        + escape_html(sanitize_utf8(song.name)) + "</a>";
    const auto album_line = song.album.empty() ? "" : "专辑：" + escape_html(sanitize_utf8(song.album)) + "\n";
    std::string info = format_file_size(song.size_bytes);
    if (song.bitrate_kbps > 0) {
        info += " · " + std::to_string(song.bitrate_kbps) + "kbps";
    }
    info += " · " + song.format;
    const auto via_line = bot_username.empty() ? "" : "\nvia @" + bot_username;
    return "「" + name_link + "」- " + escape_html(sanitize_utf8(song.artists)) + "\n"
        + album_line + info + "\n#网易云音乐 #" + escape_html(sanitize_utf8(song.level)) + via_line;
}

}  // namespace

NeteaseMessageHandler::NeteaseMessageHandler(NeteaseService &service, std::string bot_username)
    : service_{service}
    , bot_username_{std::move(bot_username)} {}

auto NeteaseMessageHandler::handle(TelegramBotClient &bot, const Message &message) -> bool {
    if (!message.text.has_value()) {
        return false;
    }
    const auto song_url = service_.extract_song_url(*message.text);
    if (!song_url.has_value()) {
        return false;
    }

    const auto progress = bot.send_message(SendMessageRequest{
        .chat_id = message.chat.id,
        .text = "正在解析并下载网易云音乐，请稍候。",
        .message_thread_id = message.message_thread_id,
    });
    if (!progress.succeeded()) {
        Log::Error("Netease progress message failed: {} body={}", progress.error_text(), progress.body);
        return true;
    }

    const auto chat_id = message.chat.id;
    const auto thread_id = message.message_thread_id;
    const auto progress_id = progress.result->message_id;
    const auto url = *song_url;

    std::jthread([&, chat_id, thread_id, progress_id, url](std::stop_token) {
        try {
            const auto song = service_.download_song(url);
            Log::Info("Netease downloaded path={} size={}", song.file_path, song.size_bytes);
            const auto upload = bot.send_audio_file(
                chat_id,
                song.file_path,
                format_song_caption(song, bot_username_),
                song.duration_seconds,
                thread_id,
                std::string{"HTML"},
                song.name,
                song.artists,
                song.thumbnail_path
            );
            if (!upload.succeeded()) {
                Log::Error("Netease sendAudio failed: {} body={}", upload.error_text(), upload.body);
                (void)bot.send_message(
                    chat_id,
                    "网易云音乐已下载，但发送到 Telegram 失败：" + upload.error_text(),
                    {},
                    thread_id
                );
            } else {
                Log::Info("Netease sendAudio ok chat={}", chat_id);
            }
            service_.delete_downloaded_file(song.file_path);
            if (song.thumbnail_path.has_value()) {
                service_.delete_downloaded_file(*song.thumbnail_path);
            }
        } catch (const std::exception &ex) {
            Log::Error("Netease job failed: {}", ex.what());
            (void)bot.send_message(
                chat_id,
                std::string{"网易云音乐处理失败："} + ex.what() + "\n原链接仍保留在聊天中：" + url,
                {},
                thread_id
            );
        }
        (void)bot.delete_message(chat_id, progress_id);
    }).detach();

    return true;
}
