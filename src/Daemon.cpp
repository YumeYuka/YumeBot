module daemon;

import bilibili.handler;
import bilibili.service;
import config;
import event.command;
import event.markup;
import event.pending;
import event.verification;
import log;
import netease.handler;
import netease.service;
import std;
import telegram;
import telegram.message;
import telegram.request;
import telegram.update;
import telegram.user;

namespace {

constexpr int k_polling_timeout_seconds = 30;
constexpr auto k_transient_backoff = std::chrono::seconds{1};

auto is_transient_telegram_error(const std::string &message) -> bool {
    return message.find("prematurely closed") != std::string::npos
        || message.find("Not enough data") != std::string::npos;
}

auto register_bot_commands(const TelegramBotClient &bot) -> void {
    const std::array commands = {
        BotCommand{.command = "start", .description = "获取验证引导"},
        BotCommand{.command = "ban", .description = "封禁违规用户 (仅管理员)"},
        BotCommand{.command = "pass", .description = "通过成员验证 (仅管理员)"},
        BotCommand{.command = "bili_login", .description = "B站扫码登录 (仅管理员)"},
    };

    const auto result = bot.set_my_commands(commands);
    if (result.succeeded()) {
        Log::Info("Bot commands registered");
    } else {
        Log::Warn(
            "Register bot commands failed: {}",
            result.description.value_or("unknown error")
        );
    }
}

auto handle_message(
    TelegramBotClient &bot,
    const Config &config,
    CommandHandler &commands,
    VerificationService &verification,
    const Message &message
) -> void {
    if (message.from.has_value()) {
        verification.pending().record_user(*message.from);
    }
    if (message.reply_to_message && (*message.reply_to_message)->from.has_value()) {
        verification.pending().record_user(*(*message.reply_to_message)->from);
    }
    if (message.new_chat_members.has_value()) {
        for (const auto &member : *message.new_chat_members) {
            verification.pending().record_user(member);
        }
    }
    if (message.left_chat_member.has_value()) {
        verification.pending().record_user(*message.left_chat_member);
    }

    if (message.new_chat_members.has_value() && !message.new_chat_members->empty()) {
        verification.handle_new_chat_members(bot, message);
        return;
    }

    if (message.left_chat_member.has_value()) {
        verification.handle_left_chat_member(bot, message);
        return;
    }

    if (message.web_app_data.has_value()) {
        verification.handle_web_app_data(bot, message);
        return;
    }

    if (!message.text.has_value()) {
        return;
    }

    (void)commands.handle(bot, config, message);
}

auto handle_update(
    TelegramBotClient &bot,
    const Config &config,
    CommandHandler &commands,
    VerificationService &verification,
    const Update &update
) -> void {
    if (update.chat_join_request.has_value()) {
        verification.handle_chat_join_request(bot, *update.chat_join_request);
        return;
    }

    if (update.chat_member.has_value()) {
        verification.pending().record_user(update.chat_member->from);
        verification.pending().record_user(update.chat_member->new_chat_member.user);
    }

    if (update.my_chat_member.has_value()) {
        verification.pending().record_user(update.my_chat_member->from);
    }

    if (update.callback_query.has_value()) {
        verification.handle_callback_query(bot, *update.callback_query);
        return;
    }

    if (update.message.has_value()) {
        handle_message(bot, config, commands, verification, *update.message);
    }
}

auto poll_updates(
    TelegramBotClient &bot,
    const Config &config,
    CommandHandler &commands,
    VerificationService &verification
) -> void {
    std::optional<std::int64_t> next_offset;

    while (true) {
        verification.expire_pending_verifications(bot);

        GetUpdatesRequest request;
        request.offset = next_offset;
        request.timeout = k_polling_timeout_seconds;

        const auto result = bot.get_updates(request);
        if (!result.succeeded()) {
            const auto message = result.description.value_or("getUpdates failed");
            if (is_transient_telegram_error(message)) {
                Log::Warn("Transient getUpdates failure: {}; retrying", message);
            } else {
                Log::Error("getUpdates failed: {}", message);
            }
            std::this_thread::sleep_for(k_transient_backoff);
            continue;
        }

        for (const auto &update : *result.result) {
            bool handled = true;
            try {
                handle_update(bot, config, commands, verification, update);
            } catch (const std::exception &ex) {
                Log::Error("Update {} failed: {}", update.update_id, ex.what());
                handled = !is_transient_telegram_error(ex.what());
            }

            if (handled) {
                next_offset = update.update_id + 1;
            } else {
                std::this_thread::sleep_for(k_transient_backoff);
                break;
            }
        }
    }
}

}  // namespace

auto Daemon::run(const Config &config) -> void {
    Log::Info("Daemon starting");

    TelegramBotClient bot{config};

    const auto me = bot.get_me();
    if (!me.succeeded()) {
        Log::Error("getMe failed: {}", me.description.value_or("unknown error"));
        return;
    }

    std::string bot_username;
    if (me.result->username.has_value()) {
        bot_username = *me.result->username;
    }

    Log::Info(
        "Bot ready: id={}, username={}, api={}",
        me.result->id,
        bot_username.empty() ? "unknown" : bot_username,
        config.telegram_api_base_url()
    );

    register_bot_commands(bot);

    PendingRepository pending;
    MarkupFactory markup{bot_username};
    VerificationService verification{bot_username, markup, pending};
    BilibiliService bilibili_service;
    NeteaseService netease_service{
        config.netease_music_u().empty() ? std::nullopt : std::optional{config.netease_music_u()},
        bot.is_local_bot_api_server(),
    };
    BilibiliMessageHandler bilibili_handler{bilibili_service};
    NeteaseMessageHandler netease_handler{netease_service, bot_username};
    CommandHandler commands{markup, pending, verification, bilibili_handler, netease_handler};

    poll_updates(bot, config, commands, verification);
}
