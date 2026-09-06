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
import telegram.chat;
import telegram.message;
import telegram.update;
import telegram.user;

namespace {

constexpr int k_polling_timeout_seconds = 30;
constexpr auto k_transient_backoff = std::chrono::seconds{1};

auto is_transient_telegram_error(const std::string &message) -> bool {
    return message.find("prematurely closed") != std::string::npos
        || message.find("Not enough data") != std::string::npos
        || message.find("Conflict") != std::string::npos;
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

struct BotRuntime {
    TelegramBotClient &bot;
    const Config &config;
    VerificationService &verification;
    CommandHandler &commands;
};

auto user_label(const User &user) -> std::string {
    if (user.username.has_value() && !user.username->empty()) {
        return std::format("@{} ({})", *user.username, user.id);
    }
    if (!user.first_name.empty()) {
        return std::format("{} ({})", user.first_name, user.id);
    }
    return std::format("id={}", user.id);
}

auto chat_label(const Chat &chat) -> std::string {
    if (chat.title.has_value() && !chat.title->empty()) {
        return std::format("{} ({})", *chat.title, chat.id);
    }
    if (chat.username.has_value() && !chat.username->empty()) {
        return std::format("@{} ({})", *chat.username, chat.id);
    }
    return std::format("{} ({})", chat.type, chat.id);
}

auto preview_text(std::string_view text) -> std::string {
    constexpr std::size_t k_max = 200;
    if (text.size() <= k_max) {
        return std::string{text};
    }
    return std::string{text.substr(0, k_max)} + "...";
}

auto log_incoming_update(const Update &update) -> void {
    if (update.chat_join_request.has_value()) {
        const auto &join = *update.chat_join_request;
        Log::Info(
            "recv chat_join_request chat={} user={}",
            chat_label(join.chat),
            user_label(join.from)
        );
        return;
    }

    if (update.callback_query.has_value()) {
        const auto &query = *update.callback_query;
        Log::Info(
            "recv callback_query from={} data={}",
            user_label(query.from),
            query.data.value_or("")
        );
        return;
    }

    if (!update.message.has_value()) {
        Log::Info("recv update id={} (unhandled type)", update.update_id);
        return;
    }

    const auto &message = *update.message;
    const auto from = message.from.has_value() ? user_label(*message.from) : std::string{"unknown"};
    if (message.new_chat_members.has_value()) {
        Log::Info(
            "recv new_chat_members chat={} from={} count={}",
            chat_label(message.chat),
            from,
            message.new_chat_members->size()
        );
        return;
    }
    if (message.left_chat_member.has_value()) {
        Log::Info(
            "recv left_chat_member chat={} user={}",
            chat_label(message.chat),
            user_label(*message.left_chat_member)
        );
        return;
    }
    if (message.web_app_data.has_value()) {
        Log::Info("recv web_app_data chat={} from={}", chat_label(message.chat), from);
        return;
    }
    Log::Info(
        "recv message chat={} from={} text={}",
        chat_label(message.chat),
        from,
        message.text.has_value() ? preview_text(*message.text) : std::string{"<non-text>"}
    );
}

auto handle_update(BotRuntime &runtime, const Update &update) -> void {
    log_incoming_update(update);
    if (update.chat_join_request.has_value()) {
        runtime.verification.handle_chat_join_request(runtime.bot, *update.chat_join_request);
        return;
    }

    if (update.callback_query.has_value()) {
        runtime.verification.handle_callback_query(runtime.bot, *update.callback_query);
        return;
    }

    if (!update.message.has_value()) {
        return;
    }

    const auto &message = *update.message;
    if (message.new_chat_members.has_value() && !message.new_chat_members->empty()) {
        runtime.verification.handle_new_chat_members(runtime.bot, message);
        return;
    }

    if (message.left_chat_member.has_value()) {
        runtime.verification.handle_left_chat_member(runtime.bot, message);
        return;
    }

    if (message.web_app_data.has_value()) {
        runtime.verification.handle_web_app_data(runtime.bot, message);
        return;
    }

    (void)runtime.commands.handle(runtime.bot, runtime.config, message);
}

auto poll_updates(BotRuntime &runtime) -> void {
    std::optional<std::int64_t> next_offset;

    while (true) {
        runtime.verification.expire_pending_verifications(runtime.bot);

        GetUpdatesRequest request;
        request.offset = next_offset;
        request.timeout = k_polling_timeout_seconds;

        const auto result = runtime.bot.get_updates(request);
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
                handle_update(runtime, update);
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
        "Bot ready: id={}, username={}, poll=https://api.telegram.org, upload={}",
        me.result->id,
        bot_username.empty() ? "unknown" : bot_username,
        bot.is_local_bot_api_server() ? config.telegram_api_base_url() : std::string{"https://api.telegram.org"}
    );

    PendingRepository pending;
    MarkupFactory markup{bot_username};
    VerificationService verification{bot_username, markup, pending};
    BilibiliService bilibili_service;
    BilibiliMessageHandler bilibili{bilibili_service};
    std::optional<std::string> music_u;
    if (!config.netease_music_u().empty()) {
        music_u = config.netease_music_u();
    }
    NeteaseService netease_service{std::move(music_u), bot.is_local_bot_api_server()};
    NeteaseMessageHandler netease{netease_service, bot_username};
    CommandHandler commands{markup, pending, verification, bilibili, netease};

    register_bot_commands(bot);

    BotRuntime runtime{
        .bot = bot,
        .config = config,
        .verification = verification,
        .commands = commands,
    };
    poll_updates(runtime);
}
