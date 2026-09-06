module daemon;

import config;
import log;
import std;
import telegram;
import telegram.markup;
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

auto handle_callback_query(TelegramBotClient &bot, const CallbackQuery &query) -> void {
    if (!query.data.has_value() || !query.message.has_value()) {
        return;
    }

    const auto &data = *query.data;
    const auto &message = *query.message;

    Log::Info(
        "callback_query: id={}, data={}, chat={}, message={}",
        query.id,
        data,
        message.chat.id,
        message.message_id
    );

    if (data.starts_with("admin_pass:") || data.starts_with("admin_ban:")) {
        (void)bot.answer_callback_query(AnswerCallbackQueryRequest{
            .callback_query_id = query.id,
            .text = std::string{"功能开发中"},
            .show_alert = true,
        });
    }
}

auto handle_message(
    TelegramBotClient &bot,
    const Config &config,
    const std::string &bot_username,
    const Message &message
) -> void {
    if (message.new_chat_members.has_value() && !message.new_chat_members->empty()) {
        Log::Info(
            "new_chat_members: chat={}, count={}",
            message.chat.id,
            message.new_chat_members->size()
        );
        return;
    }

    if (message.left_chat_member.has_value()) {
        Log::Info("left_chat_member: chat={}", message.chat.id);
        return;
    }

    if (message.web_app_data.has_value()) {
        Log::Info(
            "web_app_data: chat={}, user={}",
            message.chat.id,
            message.from.has_value() ? message.from->id : 0
        );
        return;
    }

    if (!message.text.has_value()) {
        return;
    }

    const auto &text = *message.text;
    if (text.starts_with("/start")) {
        Log::Info("/start from chat={} user={}", message.chat.id, message.from.has_value() ? message.from->id : 0);
        if (message.chat.type == "private") {
            if (config.mini_app_url().empty()) {
                (void)bot.send_message(message.chat.id, "验证入口尚未配置。");
            } else {
                (void)bot.send_message(SendMessageRequest{
                    .chat_id = message.chat.id,
                    .text = "请点击下方按钮打开验证页面，完成验证后会自动解除群内发言限制。",
                    .reply_markup = ReplyKeyboardMarkup{
                        .keyboard = {{
                            KeyboardButton{
                                .text = "打开验证页面",
                                .web_app = WebAppInfo{.url = config.mini_app_url()},
                            },
                        }},
                    },
                });
            }
        } else if (!bot_username.empty()) {
            (void)bot.send_message(SendMessageRequest{
                .chat_id = message.chat.id,
                .text = "请点击下方按钮前往机器人私聊页面完成验证。",
                .message_thread_id = message.message_thread_id,
                .reply_markup = InlineKeyboardMarkup{
                    .inline_keyboard = {{
                        InlineKeyboardButton{
                            .text = "前往机器人验证",
                            .url = "https://t.me/" + bot_username + "?start=join",
                        },
                    }},
                },
            });
        }
        return;
    }

    if (text.starts_with("/ban") || text.starts_with("/pass")) {
        Log::Info("admin command: {}", text);
    }
}

auto handle_update(
    TelegramBotClient &bot,
    const Config &config,
    const std::string &bot_username,
    const Update &update
) -> void {
    if (update.chat_join_request.has_value()) {
        const auto &join = *update.chat_join_request;
        Log::Info(
            "chat_join_request: chat={}, user={}, user_chat_id={}",
            join.chat.id,
            join.from.id,
            join.user_chat_id
        );
        return;
    }

    if (update.callback_query.has_value()) {
        handle_callback_query(bot, *update.callback_query);
        return;
    }

    if (update.message.has_value()) {
        handle_message(bot, config, bot_username, *update.message);
    }
}

auto poll_updates(TelegramBotClient &bot, const Config &config, const std::string &bot_username) -> void {
    std::optional<std::int64_t> next_offset;

    while (true) {
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
                handle_update(bot, config, bot_username, update);
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
    poll_updates(bot, config, bot_username);
}
