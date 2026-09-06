export module netease.handler;

import config;
import netease.service;
import std;
import telegram;
import telegram.message;

export class NeteaseMessageHandler {
public:
    NeteaseMessageHandler(NeteaseService &service, std::string bot_username);

    [[nodiscard]] auto handle(
        TelegramBotClient &bot,
        const Message &message
    ) -> bool;

private:
    NeteaseService &service_;
    std::string bot_username_;
};
