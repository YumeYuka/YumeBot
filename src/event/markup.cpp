module event.markup;

import std;
import telegram.markup;
import telegram.types;

MarkupFactory::MarkupFactory(std::string bot_username)
    : bot_username_{std::move(bot_username)} {}

auto MarkupFactory::inline_guide_markup(std::optional<TelegramId> target_user_id) const
    -> std::optional<InlineKeyboardMarkup> {
    if (bot_username_.empty()) {
        return std::nullopt;
    }

    const InlineKeyboardButton guide_button{
        .text = "前往机器人验证",
        .url = "https://t.me/" + bot_username_ + "?start=join",
    };

    if (!target_user_id.has_value()) {
        return InlineKeyboardMarkup{
            .inline_keyboard = {{guide_button}},
        };
    }

    return InlineKeyboardMarkup{
        .inline_keyboard = {
            {
                InlineKeyboardButton{
                    .text = "通过",
                    .callback_data = "admin_pass:" + std::to_string(*target_user_id),
                },
                InlineKeyboardButton{
                    .text = "封禁",
                    .callback_data = "admin_ban:" + std::to_string(*target_user_id),
                },
            },
            {guide_button},
        },
    };
}

auto MarkupFactory::verification_keyboard(const std::string &mini_app_url) const -> ReplyKeyboardMarkup {
    return ReplyKeyboardMarkup{
        .keyboard = {
            {
                KeyboardButton{
                    .text = "打开验证页面",
                    .web_app = WebAppInfo{.url = mini_app_url},
                },
            },
        },
    };
}
