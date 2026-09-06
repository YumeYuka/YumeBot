export module bilibili.handler;

import bilibili.service;
import config;
import std;
import telegram;
import telegram.message;

export class BilibiliMessageHandler {
public:
    BilibiliMessageHandler(BilibiliService &service);

    [[nodiscard]] auto handle(
        TelegramBotClient &bot,
        const Config &config,
        const Message &message
    ) -> bool;

private:
    BilibiliService &service_;
};
