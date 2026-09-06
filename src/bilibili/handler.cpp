module bilibili.handler;

import bilibili.service;
import common.util;
import config;
import log;
import std;
import telegram;
import telegram.message;
import telegram.request;

namespace {

auto format_video_caption(const BilibiliDownloadedVideo &video) -> std::optional<std::string> {
    if (video.summary.empty()) {
        return std::nullopt;
    }
    const auto title = escape_html(video.title.size() > 200 ? video.title.substr(0, 200) : video.title);
    const auto source = escape_html_attribute(video.source_url);
    const auto summary = escape_html(
        video.summary.size() > 500 ? video.summary.substr(0, 497) + "..." : video.summary
    );
    return "<b>" + title + "</b>\n\n" + summary + "\n\n<a href=\"" + source + "\">Source</a>";
}

}  // namespace

BilibiliMessageHandler::BilibiliMessageHandler(BilibiliService &service)
    : service_{service} {}

auto BilibiliMessageHandler::handle(
    TelegramBotClient &bot,
    const Config &config,
    const Message &message
) -> bool {
    if (!message.text.has_value()) {
        return false;
    }
    const auto &text = *message.text;

    if (text.starts_with("/bili_login")) {
        const auto operator_id = message.from.has_value() ? std::to_string(message.from->id) : std::string{};
        if (config.bilibili_admin_id().empty() || operator_id != config.bilibili_admin_id()) {
            (void)bot.send_message(SendMessageRequest{
                .chat_id = message.chat.id,
                .text = "未配置或无权使用 B 站登录命令。",
                .message_thread_id = message.message_thread_id,
            });
            return true;
        }

        try {
            const auto qr = service_.create_login_qr_code();
            const auto qr_url = "https://api.qrserver.com/v1/create-qr-code/?size=400x400&data="
                + encode_url_component(qr.login_url);
            (void)bot.send_photo(SendPhotoRequest{
                .chat_id = message.chat.id,
                .photo = qr_url,
                .caption = "请使用哔哩哔哩手机客户端扫码登录。二维码有效期约三分钟。",
                .message_thread_id = message.message_thread_id,
            });
            (void)bot.send_message(SendMessageRequest{
                .chat_id = message.chat.id,
                .text = "若扫码提示缺少参数，请直接打开此链接完成登录：\n" + qr.login_url,
                .message_thread_id = message.message_thread_id,
            });

            const auto chat_id = message.chat.id;
            const auto thread_id = message.message_thread_id;
            const auto key = qr.qr_code_key;
            std::jthread([&, chat_id, thread_id, key](std::stop_token) {
                try {
                    const auto status = service_.wait_for_login(key);
                    std::string result;
                    switch (status) {
                        case BilibiliLoginStatus::Success: result = "B站扫码登录成功，Cookie 已更新。"; break;
                        case BilibiliLoginStatus::Expired: result = "B站二维码已过期，请重新发送 /bili_login。"; break;
                        case BilibiliLoginStatus::Timeout: result = "B站扫码登录超时，请重新发送 /bili_login。"; break;
                    }
                    (void)bot.send_message(chat_id, result, {}, thread_id);
                } catch (const std::exception &ex) {
                    Log::Warn("Bilibili login failed: {}", ex.what());
                    (void)bot.send_message(
                        chat_id,
                        std::string{"B站扫码登录失败："} + ex.what(),
                        {},
                        thread_id
                    );
                }
            }).detach();
        } catch (const std::exception &ex) {
            (void)bot.send_message(SendMessageRequest{
                .chat_id = message.chat.id,
                .text = std::string{"生成 B 站二维码失败："} + ex.what(),
                .message_thread_id = message.message_thread_id,
            });
        }
        return true;
    }

    const auto video_url = service_.extract_video_url(text);
    if (!video_url.has_value()) {
        return false;
    }

    const auto progress = bot.send_message(SendMessageRequest{
        .chat_id = message.chat.id,
        .text = "正在解析并下载 B站视频，请稍候。",
        .message_thread_id = message.message_thread_id,
    });
    if (!progress.succeeded()) {
        Log::Error("Bilibili progress message failed: {} body={}", progress.error_text(), progress.body);
        return true;
    }

    const auto chat_id = message.chat.id;
    const auto thread_id = message.message_thread_id;
    const auto progress_id = progress.result->message_id;
    const auto url = *video_url;

    std::jthread([&, chat_id, thread_id, progress_id, url](std::stop_token) {
        try {
            const auto video = service_.download_video(url);
            Log::Info(
                "Bilibili downloaded path={} duration={}s",
                video.file_path,
                video.duration_seconds
            );
            const auto upload = bot.send_video_file(
                chat_id,
                video.file_path,
                format_video_caption(video),
                video.duration_seconds,
                thread_id,
                std::string{"HTML"}
            );
            if (!upload.succeeded()) {
                Log::Error("Bilibili sendVideo failed: {} body={}", upload.error_text(), upload.body);
                (void)bot.send_message(
                    chat_id,
                    "B站视频已下载，但发送到 Telegram 失败：" + upload.error_text(),
                    {},
                    thread_id
                );
            } else {
                Log::Info("Bilibili sendVideo ok chat={}", chat_id);
            }
            service_.delete_downloaded_file(video.file_path);
        } catch (const std::exception &ex) {
            Log::Error("Bilibili job failed: {}", ex.what());
            (void)bot.send_message(
                chat_id,
                std::string{"B站视频处理失败："} + ex.what() + "\n原链接仍保留在聊天中：" + url,
                {},
                thread_id
            );
        }
        (void)bot.delete_message(chat_id, progress_id);
    }).detach();

    return true;
}
