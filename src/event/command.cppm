export module event.command;

import bilibili.handler;
import config;
import event.markup;
import event.pending;
import event.verification;
import netease.handler;
import std;
import telegram;
import telegram.message;

export class CommandHandler {
public:
    CommandHandler(
        MarkupFactory markup,
        PendingRepository &pending,
        VerificationService &verification,
        BilibiliMessageHandler &bilibili,
        NeteaseMessageHandler &netease
    );

    [[nodiscard]] auto handle(
        TelegramBotClient &bot,
        const Config &config,
        const Message &message
    ) -> bool;

private:
    MarkupFactory markup_;
    PendingRepository &pending_;
    VerificationService &verification_;
    BilibiliMessageHandler &bilibili_;
    NeteaseMessageHandler &netease_;

    auto send_start_guide(
        TelegramBotClient &bot,
        const Config &config,
        const Message &message
    ) -> void;

    auto handle_ban(TelegramBotClient &bot, const Message &message) -> void;

    auto handle_pass(TelegramBotClient &bot, const Message &message) -> void;
};
