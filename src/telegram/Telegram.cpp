module telegram;

import std;
import config;
import http;
import http.response;
import log;
import telegram.join;
import telegram.json;
import telegram.member;
import telegram.message;
import telegram.request;
import telegram.types;
import telegram.update;
import telegram.user;

namespace {

auto default_api_base(std::string api_base_url) -> std::string {
    if (api_base_url.empty()) {
        return "https://api.telegram.org";
    }
    while (api_base_url.ends_with('/')) {
        api_base_url.pop_back();
    }
    return api_base_url;
}

auto is_official_host(std::string_view base) -> bool {
    return base == "https://api.telegram.org" || base == "http://api.telegram.org";
}

auto resolve_local_file_path(std::string_view path) -> std::string {
    std::filesystem::path file{std::string{path}};
    std::error_code ec;
    auto absolute = std::filesystem::absolute(file, ec);
    if (ec) {
        return std::string{path};
    }
    auto canonical = std::filesystem::weakly_canonical(absolute, ec);
    auto resolved = (ec ? absolute : canonical).generic_string();
    if (!std::filesystem::exists(absolute) && !std::filesystem::exists(canonical)) {
        Log::Warn("Local Bot API file does not exist: {}", resolved);
    }
    return resolved;
}

auto read_envelope(const HttpResponse &http) -> JsonValue {
    if (!http.error.empty()) {
        JsonValue::Object object;
        json_put(object, "ok", false);
        json_put(object, "description", http.error);
        return JsonValue::object(std::move(object));
    }
    if (http.body.empty()) {
        JsonValue::Object object;
        json_put(object, "ok", false);
        json_put(object, "description", "empty telegram response");
        return JsonValue::object(std::move(object));
    }
    return JsonValue::parse(http.body);
}

template<typename T>
auto envelope_to_result(const HttpResponse &http, auto &&decode) -> TelegramResult<T> {
    TelegramResult<T> out;
    out.body = http.body;
    try {
        const auto json = read_envelope(http);
        out.ok = json_bool(json, "ok").value_or(false);
        out.description = json_string(json, "description");
        if (const auto code = json_i64(json, "error_code")) {
            out.error_code = static_cast<int>(*code);
        }
        if (out.ok) {
            if (const auto *result = json.get("result")) {
                out.result = decode(*result);
            }
        }
    } catch (const std::exception &ex) {
        out.ok = false;
        out.description = ex.what();
    }
    return out;
}

auto json_header() -> std::array<std::pair<std::string, std::string>, 1> {
    return {std::pair<std::string, std::string>{"Content-Type", "application/json"}};
}

}  // namespace

TelegramBotClient::TelegramBotClient(Config config)
    : TelegramBotClient{config.bot_token(), config.telegram_api_base_url()} {}

TelegramBotClient::TelegramBotClient(std::string token, std::string api_base_url)
    : token_{std::move(token)}
    , api_base_url_{"https://api.telegram.org"}
    , upload_api_base_url_{default_api_base(std::move(api_base_url))}
    , local_server_{!is_official_host(upload_api_base_url_)} {}

auto TelegramBotClient::method_url(std::string_view method) const -> std::string {
    std::string url;
    url.reserve(api_base_url_.size() + token_.size() + method.size() + 8);
    url += api_base_url_;
    url += "/bot";
    url += token_;
    url += '/';
    url += method;
    return url;
}

auto TelegramBotClient::file_method_url(std::string_view method) const -> std::string {
    const auto &base = local_server_ ? upload_api_base_url_ : api_base_url_;
    std::string url;
    url.reserve(base.size() + token_.size() + method.size() + 8);
    url += base;
    url += "/bot";
    url += token_;
    url += '/';
    url += method;
    return url;
}

auto TelegramBotClient::execute_message(std::string_view method, const JsonValue &body) const -> TelegramResult<Message> {
    const auto headers = json_header();
    const auto http = http_.post(method_url(method), body.dump(), headers);
    return envelope_to_result<Message>(http, [](const JsonValue &json) { return Message::from_json(json); });
}

auto TelegramBotClient::execute_bool(std::string_view method, const JsonValue &body) const -> TelegramResult<bool> {
    const auto headers = json_header();
    const auto http = http_.post(method_url(method), body.dump(), headers);
    return envelope_to_result<bool>(http, [](const JsonValue &json) { return json.as_bool().value_or(true); });
}

auto TelegramBotClient::get_me() const -> TelegramResult<User> {
    const auto headers = json_header();
    const auto http = http_.post(method_url("getMe"), "{}", headers);
    return envelope_to_result<User>(http, [](const JsonValue &json) { return User::from_json(json); });
}

auto TelegramBotClient::set_my_commands(std::span<const BotCommand> commands) const -> TelegramResult<bool> {
    JsonValue::Array items;
    for (const auto &command : commands) {
        items.push_back(command.to_json());
    }
    JsonValue::Object object;
    json_put(object, "commands", JsonValue::array(std::move(items)));
    return execute_bool("setMyCommands", JsonValue::object(std::move(object)));
}

auto TelegramBotClient::send_message(const SendMessageRequest &request) const -> TelegramResult<Message> {
    return execute_message("sendMessage", request.to_json());
}

auto TelegramBotClient::send_message(
    TelegramId chat_id,
    std::string text,
    std::optional<std::string> parse_mode,
    std::optional<std::int64_t> message_thread_id
) const -> TelegramResult<Message> {
    return send_message(SendMessageRequest{
        .chat_id = chat_id,
        .text = std::move(text),
        .parse_mode = std::move(parse_mode),
        .message_thread_id = message_thread_id,
    });
}

auto TelegramBotClient::send_photo(const SendPhotoRequest &request) const -> TelegramResult<Message> {
    return execute_message("sendPhoto", request.to_json());
}

auto TelegramBotClient::send_document(const SendDocumentRequest &request) const -> TelegramResult<Message> {
    return execute_message("sendDocument", request.to_json());
}

auto TelegramBotClient::send_media_file(
    std::string_view method,
    std::string_view file_field,
    TelegramId chat_id,
    std::string_view file_path,
    std::optional<std::string> caption,
    std::optional<std::int64_t> message_thread_id,
    std::optional<int> duration_seconds,
    std::optional<std::string> parse_mode,
    std::optional<std::string> title,
    std::optional<std::string> performer,
    std::optional<std::string> thumbnail_path
) const -> TelegramResult<Message> {
    if (local_server_) {
        const auto absolute_path = resolve_local_file_path(file_path);
        std::optional<std::string> absolute_thumbnail;
        if (thumbnail_path.has_value()) {
            absolute_thumbnail = resolve_local_file_path(*thumbnail_path);
        }
        Log::Info(
            "upload {} via local Bot API path={} size={}",
            method,
            absolute_path,
            std::filesystem::exists(absolute_path)
                ? std::to_string(std::filesystem::file_size(absolute_path))
                : std::string{"missing"}
        );
        JsonValue::Object object;
        json_put(object, "chat_id", chat_id);
        json_put(object, std::string{file_field}, absolute_path);
        json_put(object, "caption", caption);
        json_put(object, "parse_mode", parse_mode);
        json_put(object, "message_thread_id", message_thread_id);
        json_put(object, "duration", duration_seconds);
        json_put(object, "title", title);
        json_put(object, "performer", performer);
        if (method == "sendVideo") {
            json_put(object, "supports_streaming", true);
        }
        if (absolute_thumbnail.has_value()) {
            json_put(object, "thumbnail", *absolute_thumbnail);
        }
        const auto headers = json_header();
        const auto http = http_.post(file_method_url(method), JsonValue::object(std::move(object)).dump(), headers);
        auto result = envelope_to_result<Message>(http, [](const JsonValue &json) { return Message::from_json(json); });
        if (!result.succeeded()) {
            Log::Warn("Telegram {} failed: {} body={}", method, result.error_text(), result.body);
        }
        return result;
    }

    std::vector<HttpMultipartPart> parts;
    parts.push_back(HttpMultipartPart{
        .name = "chat_id",
        .value = std::to_string(chat_id),
        .file_path = {},
    });
    parts.push_back(HttpMultipartPart{
        .name = std::string{file_field},
        .value = {},
        .file_path = std::string{file_path},
    });
    if (caption.has_value()) {
        parts.push_back(HttpMultipartPart{.name = "caption", .value = *caption, .file_path = {}});
    }
    if (parse_mode.has_value()) {
        parts.push_back(HttpMultipartPart{.name = "parse_mode", .value = *parse_mode, .file_path = {}});
    }
    if (message_thread_id.has_value()) {
        parts.push_back(HttpMultipartPart{
            .name = "message_thread_id",
            .value = std::to_string(*message_thread_id),
            .file_path = {},
        });
    }
    if (duration_seconds.has_value()) {
        parts.push_back(HttpMultipartPart{
            .name = "duration",
            .value = std::to_string(*duration_seconds),
            .file_path = {},
        });
    }
    if (title.has_value()) {
        parts.push_back(HttpMultipartPart{.name = "title", .value = *title, .file_path = {}});
    }
    if (performer.has_value()) {
        parts.push_back(HttpMultipartPart{.name = "performer", .value = *performer, .file_path = {}});
    }
    if (thumbnail_path.has_value()) {
        parts.push_back(HttpMultipartPart{.name = "thumbnail", .value = {}, .file_path = *thumbnail_path});
    }

    const auto http = http_.post_multipart(file_method_url(method), parts);
    return envelope_to_result<Message>(http, [](const JsonValue &json) { return Message::from_json(json); });
}

auto TelegramBotClient::send_document_file(
    TelegramId chat_id,
    std::string_view file_path,
    std::optional<std::string> caption,
    std::optional<std::int64_t> message_thread_id
) const -> TelegramResult<Message> {
    return send_media_file("sendDocument", "document", chat_id, file_path, std::move(caption), message_thread_id, {});
}

auto TelegramBotClient::send_photo_file(
    TelegramId chat_id,
    std::string_view file_path,
    std::optional<std::string> caption,
    std::optional<std::int64_t> message_thread_id
) const -> TelegramResult<Message> {
    return send_media_file("sendPhoto", "photo", chat_id, file_path, std::move(caption), message_thread_id, {});
}

auto TelegramBotClient::send_video_file(
    TelegramId chat_id,
    std::string_view file_path,
    std::optional<std::string> caption,
    std::optional<int> duration_seconds,
    std::optional<std::int64_t> message_thread_id,
    std::optional<std::string> parse_mode
) const -> TelegramResult<Message> {
    return send_media_file(
        "sendVideo",
        "video",
        chat_id,
        file_path,
        std::move(caption),
        message_thread_id,
        duration_seconds,
        std::move(parse_mode)
    );
}

auto TelegramBotClient::send_audio_file(
    TelegramId chat_id,
    std::string_view file_path,
    std::optional<std::string> caption,
    std::optional<int> duration_seconds,
    std::optional<std::int64_t> message_thread_id,
    std::optional<std::string> parse_mode,
    std::optional<std::string> title,
    std::optional<std::string> performer,
    std::optional<std::string> thumbnail_path
) const -> TelegramResult<Message> {
    return send_media_file(
        "sendAudio",
        "audio",
        chat_id,
        file_path,
        std::move(caption),
        message_thread_id,
        duration_seconds,
        std::move(parse_mode),
        std::move(title),
        std::move(performer),
        std::move(thumbnail_path)
    );
}

auto TelegramBotClient::delete_message(const DeleteMessageRequest &request) const -> TelegramResult<bool> {
    return execute_bool("deleteMessage", request.to_json());
}

auto TelegramBotClient::delete_message(TelegramId chat_id, std::int64_t message_id) const -> TelegramResult<bool> {
    return delete_message(DeleteMessageRequest{.chat_id = chat_id, .message_id = message_id});
}

auto TelegramBotClient::answer_callback_query(const AnswerCallbackQueryRequest &request) const -> TelegramResult<bool> {
    return execute_bool("answerCallbackQuery", request.to_json());
}

auto TelegramBotClient::get_updates(const GetUpdatesRequest &request) const -> TelegramResult<std::vector<Update>> {
    const auto headers = json_header();
    const auto http = http_.post(method_url("getUpdates"), request.to_json().dump(), headers);
    return envelope_to_result<std::vector<Update>>(http, [](const JsonValue &json) {
        std::vector<Update> updates;
        if (const auto *items = json.as_array()) {
            updates.reserve(items->size());
            for (const auto &item : *items) {
                updates.push_back(Update::from_json(item));
            }
        }
        return updates;
    });
}

auto TelegramBotClient::get_chat_member(const GetChatMemberRequest &request) const -> TelegramResult<ChatMember> {
    const auto headers = json_header();
    const auto http = http_.post(method_url("getChatMember"), request.to_json().dump(), headers);
    return envelope_to_result<ChatMember>(http, [](const JsonValue &json) { return ChatMember::from_json(json); });
}

auto TelegramBotClient::get_user_profile_photos(
    const GetUserProfilePhotosRequest &request
) const -> TelegramResult<UserProfilePhotos> {
    const auto headers = json_header();
    const auto http = http_.post(method_url("getUserProfilePhotos"), request.to_json().dump(), headers);
    return envelope_to_result<UserProfilePhotos>(http, [](const JsonValue &json) {
        return UserProfilePhotos::from_json(json);
    });
}

auto TelegramBotClient::ban_chat_member(const BanChatMemberRequest &request) const -> TelegramResult<bool> {
    return execute_bool("banChatMember", request.to_json());
}

auto TelegramBotClient::unban_chat_member(const UnbanChatMemberRequest &request) const -> TelegramResult<bool> {
    return execute_bool("unbanChatMember", request.to_json());
}

auto TelegramBotClient::restrict_chat_member(const RestrictChatMemberRequest &request) const -> TelegramResult<bool> {
    return execute_bool("restrictChatMember", request.to_json());
}

auto TelegramBotClient::create_chat_invite_link(const CreateChatInviteLinkRequest &request) const -> TelegramResult<ChatInviteLink> {
    const auto headers = json_header();
    const auto http = http_.post(method_url("createChatInviteLink"), request.to_json().dump(), headers);
    return envelope_to_result<ChatInviteLink>(http, [](const JsonValue &json) { return ChatInviteLink::from_json(json); });
}

auto TelegramBotClient::approve_chat_join_request(const ApproveChatJoinRequest &request) const -> TelegramResult<bool> {
    return execute_bool("approveChatJoinRequest", request.to_json());
}

auto TelegramBotClient::decline_chat_join_request(const DeclineChatJoinRequest &request) const -> TelegramResult<bool> {
    return execute_bool("declineChatJoinRequest", request.to_json());
}
