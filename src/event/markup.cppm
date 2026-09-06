export module event.markup;

import std;
import telegram.markup;
import telegram.types;

export class MarkupFactory {
public:
    explicit MarkupFactory(std::string bot_username);

    [[nodiscard]] auto inline_guide_markup(
        std::optional<TelegramId> target_user_id = std::nullopt
    ) const -> std::optional<InlineKeyboardMarkup>;

    [[nodiscard]] auto verification_keyboard(const std::string &mini_app_url) const -> ReplyKeyboardMarkup;

private:
    std::string bot_username_;
};
