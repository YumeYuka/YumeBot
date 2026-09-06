export module telegram;

import std;
import config;
import http;
import telegram.join;
import telegram.json;
import telegram.member;
import telegram.message;
import telegram.request;
import telegram.types;
import telegram.update;
import telegram.user;

export class TelegramBotClient {
public:
    explicit TelegramBotClient(Config config);

    TelegramBotClient(std::string token, std::string api_base_url);

    [[nodiscard]] auto is_local_bot_api_server() const noexcept -> bool {
        return local_server_;
    }

    [[nodiscard]] auto get_me() const -> TelegramResult<User>;

    [[nodiscard]] auto set_my_commands(std::span<const BotCommand> commands) const -> TelegramResult<bool>;

    [[nodiscard]] auto send_message(const SendMessageRequest &request) const -> TelegramResult<Message>;

    [[nodiscard]] auto send_message(
        TelegramId chat_id,
        std::string text,
        std::optional<std::string> parse_mode = {},
        std::optional<std::int64_t> message_thread_id = {}
    ) const -> TelegramResult<Message>;

    [[nodiscard]] auto send_photo(const SendPhotoRequest &request) const -> TelegramResult<Message>;

    [[nodiscard]] auto send_document(const SendDocumentRequest &request) const -> TelegramResult<Message>;

    [[nodiscard]] auto send_document_file(
        TelegramId chat_id,
        std::string_view file_path,
        std::optional<std::string> caption = {},
        std::optional<std::int64_t> message_thread_id = {}
    ) const -> TelegramResult<Message>;

    [[nodiscard]] auto send_photo_file(
        TelegramId chat_id,
        std::string_view file_path,
        std::optional<std::string> caption = {},
        std::optional<std::int64_t> message_thread_id = {}
    ) const -> TelegramResult<Message>;

    [[nodiscard]] auto send_video_file(
        TelegramId chat_id,
        std::string_view file_path,
        std::optional<std::string> caption = {},
        std::optional<int> duration_seconds = {},
        std::optional<std::int64_t> message_thread_id = {},
        std::optional<std::string> parse_mode = {}
    ) const -> TelegramResult<Message>;

    [[nodiscard]] auto send_audio_file(
        TelegramId chat_id,
        std::string_view file_path,
        std::optional<std::string> caption = {},
        std::optional<int> duration_seconds = {},
        std::optional<std::int64_t> message_thread_id = {},
        std::optional<std::string> parse_mode = {},
        std::optional<std::string> title = {},
        std::optional<std::string> performer = {},
        std::optional<std::string> thumbnail_path = {}
    ) const -> TelegramResult<Message>;

    [[nodiscard]] auto delete_message(const DeleteMessageRequest &request) const -> TelegramResult<bool>;

    [[nodiscard]] auto delete_message(TelegramId chat_id, std::int64_t message_id) const -> TelegramResult<bool>;

    [[nodiscard]] auto answer_callback_query(const AnswerCallbackQueryRequest &request) const -> TelegramResult<bool>;

    [[nodiscard]] auto get_updates(const GetUpdatesRequest &request = {}) const -> TelegramResult<std::vector<Update> >;

    [[nodiscard]] auto get_chat_member(const GetChatMemberRequest &request) const -> TelegramResult<ChatMember>;

    [[nodiscard]] auto get_user_profile_photos(
        const GetUserProfilePhotosRequest &request
    ) const -> TelegramResult<UserProfilePhotos>;

    [[nodiscard]] auto ban_chat_member(const BanChatMemberRequest &request) const -> TelegramResult<bool>;

    [[nodiscard]] auto unban_chat_member(const UnbanChatMemberRequest &request) const -> TelegramResult<bool>;

    [[nodiscard]] auto restrict_chat_member(const RestrictChatMemberRequest &request) const -> TelegramResult<bool>;

    [[nodiscard]] auto create_chat_invite_link(
        const CreateChatInviteLinkRequest &request) const -> TelegramResult<ChatInviteLink>;

    [[nodiscard]] auto approve_chat_join_request(const ApproveChatJoinRequest &request) const -> TelegramResult<bool>;

    [[nodiscard]] auto decline_chat_join_request(const DeclineChatJoinRequest &request) const -> TelegramResult<bool>;

private:
    HttpClient http_;
    std::string token_;
    std::string api_base_url_;
    bool local_server_{false};

    [[nodiscard]] auto method_url(std::string_view method) const -> std::string;

    [[nodiscard]] auto execute_message(std::string_view method, const JsonValue &body) const -> TelegramResult<Message>;

    [[nodiscard]] auto execute_bool(std::string_view method, const JsonValue &body) const -> TelegramResult<bool>;

    [[nodiscard]] auto send_media_file(
        std::string_view method,
        std::string_view file_field,
        TelegramId chat_id,
        std::string_view file_path,
        std::optional<std::string> caption,
        std::optional<std::int64_t> message_thread_id,
        std::optional<int> duration_seconds,
        std::optional<std::string> parse_mode = {},
        std::optional<std::string> title = {},
        std::optional<std::string> performer = {},
        std::optional<std::string> thumbnail_path = {}
    ) const -> TelegramResult<Message>;
};
