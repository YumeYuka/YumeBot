module event.command;

import bilibili.handler;
import config;
import event.markup;
import event.pending;
import event.verification;
import log;
import netease.handler;
import std;
import telegram;
import telegram.markup;
import telegram.member;
import telegram.message;
import telegram.request;
import telegram.types;

namespace {
    auto send_reply(
        TelegramBotClient &bot,
        const Message &message,
        std::string text,
        std::optional<ReplyMarkup> reply_markup = {}
    ) -> void {
        SendMessageRequest request{
            .chat_id = message.chat.id,
            .text = std::move(text),
            .message_thread_id = message.message_thread_id,
        };
        if (reply_markup.has_value()) {
            request.reply_markup = std::move(*reply_markup);
        }
        (void) bot.send_message(request);
    }

    auto delete_message_quietly(
        TelegramBotClient &bot,
        TelegramId chat_id,
        std::int64_t message_id
    ) -> void {
        const auto result = bot.delete_message(chat_id, message_id);
        if (!result.succeeded()) {
            Log::Warn(
                "Delete message failed: chat={}, message={}, error={}",
                chat_id,
                message_id,
                result.description.value_or("unknown error")
            );
        }
    }

    auto can_restrict_members(TelegramBotClient &bot, const Message &message) -> bool {
        if (!message.from.has_value()) {
            return false;
        }

        const auto member = bot.get_chat_member(GetChatMemberRequest{
            .chat_id = message.chat.id,
            .user_id = message.from->id,
        });
        if (!member.succeeded()) {
            return false;
        }

        return member.result->status == "creator"
               || (member.result->status == "administrator"
                   && member.result->can_restrict_members.value_or(false));
    }

    auto extract_mentioned_username(const Message &message, const std::string &text) -> std::optional<std::string> {
        if (!message.entities.has_value()) {
            return std::nullopt;
        }

        for (const auto &entity: *message.entities) {
            if (entity.type != "mention") {
                continue;
            }
            if (entity.offset < 0 || entity.length <= 0) {
                continue;
            }
            const auto begin = static_cast<std::size_t>(entity.offset);
            const auto end = begin + static_cast<std::size_t>(entity.length);
            if (end > text.size()) {
                continue;
            }
            auto username = text.substr(begin, static_cast<std::size_t>(entity.length));
            if (username.starts_with('@')) {
                username.erase(0, 1);
            }
            return username;
        }

        return std::nullopt;
    }

    auto extract_text_mention_user_id(const Message &message) -> std::optional<TelegramId> {
        if (!message.entities.has_value()) {
            return std::nullopt;
        }

        for (const auto &entity: *message.entities) {
            if (entity.type == "text_mention" && entity.user.has_value()) {
                return entity.user->id;
            }
        }

        return std::nullopt;
    }

    auto extract_command_argument(const std::string &text) -> std::optional<std::string_view> {
        const auto space = text.find_first_of(" \t\r\n");
        if (space == std::string::npos || space + 1 >= text.size()) {
            return std::nullopt;
        }

        auto argument = std::string_view{text}.substr(space + 1);
        while (!argument.empty() && std::isspace(static_cast<unsigned char>(argument.front()))) {
            argument.remove_prefix(1);
        }
        while (!argument.empty() && std::isspace(static_cast<unsigned char>(argument.back()))) {
            argument.remove_suffix(1);
        }
        if (argument.empty()) {
            return std::nullopt;
        }
        return argument;
    }

    auto parse_int64(std::string_view text) -> std::optional<std::int64_t> {
        try {
            std::size_t consumed = 0;
            const auto value = std::stoll(std::string{text}, &consumed);
            if (consumed != text.size()) {
                return std::nullopt;
            }
            return value;
        } catch (const std::exception &) {
            return std::nullopt;
        }
    }

    auto resolve_target_user_id(const Message &message, PendingRepository &pending) -> std::optional<TelegramId> {
        if (message.reply_to_message
            && (*message.reply_to_message)->from.has_value()) {
            return (*message.reply_to_message)->from->id;
        }

        if (const auto text_mention = extract_text_mention_user_id(message)) {
            return text_mention;
        }

        if (!message.text.has_value()) {
            return std::nullopt;
        }

        const auto &text = *message.text;
        auto mentioned = extract_mentioned_username(message, text);
        if (!mentioned.has_value()) {
            if (const auto argument = extract_command_argument(text)) {
                auto value = std::string{*argument};
                if (value.starts_with('@')) {
                    value.erase(0, 1);
                }
                mentioned = std::move(value);
            }
        }

        if (!mentioned.has_value() || mentioned->empty()) {
            return std::nullopt;
        }

        if (const auto direct_id = parse_int64(*mentioned)) {
            return direct_id;
        }

        return pending.find_user_id_by_username(*mentioned);
    }
} // namespace

CommandHandler::CommandHandler(
    MarkupFactory markup,
    PendingRepository &pending,
    VerificationService &verification,
    BilibiliMessageHandler &bilibili,
    NeteaseMessageHandler &netease
)
    : markup_{std::move(markup)}
      , pending_{pending}
      , verification_{verification}
      , bilibili_{bilibili}
      , netease_{netease} {}

auto CommandHandler::handle(
    TelegramBotClient &bot,
    const Config &config,
    const Message &message
) -> bool {
    if (!message.text.has_value()) {
        return false;
    }

    if (message.from.has_value()) {
        pending_.record_user(*message.from);
    }

    auto trimmed = *message.text;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) {
        trimmed.erase(trimmed.begin());
    }
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
        trimmed.pop_back();
    }

    if (trimmed.empty()) {
        return false;
    }

    if (trimmed.starts_with("/start")) {
        send_start_guide(bot, config, message);
        return true;
    }

    if (bilibili_.handle(bot, config, message)) {
        return true;
    }

    if (netease_.handle(bot, message)) {
        return true;
    }

    if (trimmed.starts_with("/ban")) {
        handle_ban(bot, message);
        return true;
    }

    if (trimmed.starts_with("/pass")
        || trimmed.starts_with("/approve")
        || trimmed.starts_with("/通过")) {
        handle_pass(bot, message);
        return true;
    }

    return false;
}

auto CommandHandler::send_start_guide(
    TelegramBotClient &bot,
    const Config &config,
    const Message &message
) -> void {
    if (message.chat.type != "private") {
        if (const auto markup = markup_.inline_guide_markup()) {
            send_reply(
                bot,
                message,
                "请点击下方按钮前往机器人私聊页面完成验证测试。",
                *markup
            );
        }
        return;
    }

    if (config.mini_app_url().empty()) {
        send_reply(bot, message, "验证入口尚未配置。");
        return;
    }

    send_reply(
        bot,
        message,
        "请点击下方按钮打开验证页面，完成验证后会自动解除群内发言限制。",
        markup_.verification_keyboard(config.mini_app_url())
    );
}

auto CommandHandler::handle_ban(TelegramBotClient &bot, const Message &message) -> void {
    if (!can_restrict_members(bot, message)) {
        send_reply(bot, message, "只有拥有封禁成员权限的管理员可以使用 /ban。");
        return;
    }

    const auto target_user_id = resolve_target_user_id(message, pending_);
    if (!target_user_id.has_value()) {
        send_reply(
            bot,
            message,
            "用法：回复目标用户的消息发送 /ban，或直接指定用户：/ban @用户名 或 /ban <用户ID>。"
        );
        return;
    }

    const auto ban_target_message_id =
            message.reply_to_message
                ? std::optional{(*message.reply_to_message)->message_id}
                : std::nullopt;

    if (!verification_.manual_ban_member(
        bot,
        message.chat.id,
        *target_user_id,
        message.from.has_value() ? std::optional{message.from->id} : std::nullopt
    )) {
        send_reply(bot, message, "封禁失败。");
        return;
    }

    delete_message_quietly(bot, message.chat.id, message.message_id);
    if (ban_target_message_id.has_value()) {
        delete_message_quietly(bot, message.chat.id, *ban_target_message_id);
    }
}

auto CommandHandler::handle_pass(TelegramBotClient &bot, const Message &message) -> void {
    if (!can_restrict_members(bot, message)) {
        send_reply(bot, message, "只有拥有封禁/限制成员权限的管理员可以使用 /通过。");
        return;
    }

    const auto target_user_id = resolve_target_user_id(message, pending_);
    if (!target_user_id.has_value()) {
        send_reply(
            bot,
            message,
            "用法：回复目标用户的消息发送 /通过，或直接指定用户：/通过 @用户名 或 /通过 <用户ID>。"
        );
        return;
    }

    if (!verification_.manual_approve_member(
        bot,
        message.chat.id,
        *target_user_id,
        message.from.has_value() ? std::optional{message.from->id} : std::nullopt
    )) {
        send_reply(bot, message, "通过验证失败，该用户已被管理员封禁或无法操作。");
        return;
    }

    delete_message_quietly(bot, message.chat.id, message.message_id);
}
