export module event.verification;

import config;
import event.markup;
import event.pending;
import event.verification_payload;
import std;
import telegram;
import telegram.join;
import telegram.message;
import telegram.member;
import telegram.types;
import telegram.update;
import telegram.user;

export class VerificationService {
public:
    VerificationService(std::string bot_username, MarkupFactory markup, PendingRepository &pending);

    auto handle_chat_join_request(TelegramBotClient &bot, const ChatJoinRequest &join_request) -> void;

    auto handle_new_chat_members(TelegramBotClient &bot, const Message &message) -> void;

    auto handle_chat_member(TelegramBotClient &bot, const ChatMemberUpdated &update) -> void;

    auto handle_left_chat_member(TelegramBotClient &bot, const Message &message) -> void;

    auto handle_callback_query(TelegramBotClient &bot, const CallbackQuery &query) -> void;

    auto handle_web_app_data(TelegramBotClient &bot, const Message &message) -> void;

    auto expire_pending_verifications(TelegramBotClient &bot) -> void;

    [[nodiscard]] auto pending() const -> PendingRepository & { return pending_; }

    [[nodiscard]] auto manual_approve_member(
        TelegramBotClient &bot,
        TelegramId chat_id,
        TelegramId target_user_id,
        std::optional<TelegramId> admin_id = {}
    ) -> bool;

    [[nodiscard]] auto manual_ban_member(
        TelegramBotClient &bot,
        TelegramId chat_id,
        TelegramId target_user_id,
        std::optional<TelegramId> admin_id = {}
    ) -> bool;

private:
    auto begin_group_verification(
        TelegramBotClient &bot,
        TelegramId chat_id,
        const User &member,
        std::optional<std::int64_t> message_thread_id
    ) -> void;

    std::string bot_username_;
    MarkupFactory markup_;
    PendingRepository &pending_;
};
