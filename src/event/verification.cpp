module event.verification;

import config;
import event.markup;
import event.pending;
import event.profile_screen;
import event.verification_payload;
import log;
import std;
import telegram;
import telegram.join;
import telegram.markup;
import telegram.member;
import telegram.message;
import telegram.request;
import telegram.types;
import telegram.update;
import telegram.user;

namespace {

constexpr std::int64_t k_verification_timeout_millis = 5 * 60 * 1000;
constexpr UnixTime k_permanent_ban_until_date = 0;

auto now_millis() -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

auto escape_html(const std::string &text) -> std::string {
    std::string escaped;
    escaped.reserve(text.size());
    for (const char ch : text) {
        switch (ch) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            default: escaped += ch; break;
        }
    }
    return escaped;
}

auto is_in_chat(const ChatMember &member) -> bool {
    if (member.status == "left" || member.status == "kicked") {
        return false;
    }
    if (member.status == "restricted") {
        return member.is_member.value_or(false);
    }
    return member.status == "member"
        || member.status == "administrator"
        || member.status == "creator";
}

auto muted_permissions() -> ChatPermissions {
    return ChatPermissions{
        .can_send_messages = false,
        .can_send_audios = false,
        .can_send_documents = false,
        .can_send_photos = false,
        .can_send_videos = false,
        .can_send_video_notes = false,
        .can_send_voice_notes = false,
        .can_send_polls = false,
        .can_send_other_messages = false,
        .can_add_web_page_previews = false,
    };
}

auto unmuted_permissions() -> ChatPermissions {
    return ChatPermissions{
        .can_send_messages = true,
        .can_send_audios = true,
        .can_send_documents = true,
        .can_send_photos = true,
        .can_send_videos = true,
        .can_send_video_notes = true,
        .can_send_voice_notes = true,
        .can_send_polls = true,
        .can_send_other_messages = true,
        .can_add_web_page_previews = true,
        .can_invite_users = true,
    };
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

auto can_restrict_members(
    TelegramBotClient &bot,
    TelegramId chat_id,
    TelegramId operator_user_id
) -> bool {
    const auto member = bot.get_chat_member(GetChatMemberRequest{
        .chat_id = chat_id,
        .user_id = operator_user_id,
    });
    if (!member.succeeded()) {
        Log::Warn(
            "Check admin permission failed: chat={}, user={}, error={}",
            chat_id,
            operator_user_id,
            member.description.value_or("unknown error")
        );
        return false;
    }

    return member.result->status == "creator"
        || (member.result->status == "administrator"
            && member.result->can_restrict_members.value_or(false));
}

auto build_group_prompt(const User &member) -> std::string {
    const std::string mention = member.username.has_value() && !member.username->empty()
        ? "@" + *member.username
        : "<a href=\"tg://user?id=" + std::to_string(member.id) + "\">"
            + escape_html(member.first_name) + "</a>";
    return mention + " 欢迎加入。请在 5 分钟内点击下方按钮完成验证，超时会被移出群组。";
}

auto build_join_request_prompt() -> std::string {
    return "收到你的入群申请。\n请在 5 分钟内点击下方按钮前往机器人私聊页面，然后打开验证按钮完成验证，超时将拒绝申请。";
}

auto cleanup_prompt_messages(TelegramBotClient &bot, const PendingJoinRequest &pending) -> void {
    if (pending.prompt_message_id.has_value()) {
        delete_message_quietly(bot, pending.prompt_chat_id, *pending.prompt_message_id);
    }
    if (pending.guide_chat_id.has_value() && pending.guide_message_id.has_value()) {
        delete_message_quietly(bot, *pending.guide_chat_id, *pending.guide_message_id);
    }
}

auto parse_callback_target_user_id(std::string_view data, std::string_view prefix) -> std::optional<TelegramId> {
    if (!data.starts_with(prefix)) {
        return std::nullopt;
    }
    const auto raw = data.substr(prefix.size());
    try {
        std::size_t consumed = 0;
        const auto value = std::stoll(std::string{raw}, &consumed);
        if (consumed != raw.size()) {
            return std::nullopt;
        }
        return value;
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

auto has_user_avatar(TelegramBotClient &bot, TelegramId user_id) -> bool {
    const auto photos = bot.get_user_profile_photos(GetUserProfilePhotosRequest{
        .user_id = user_id,
        .limit = 1,
    });
    if (!photos.succeeded()) {
        Log::Warn(
            "getUserProfilePhotos failed: user={}, error={}",
            user_id,
            photos.description.value_or("unknown error")
        );
        return true;
    }
    return photos.result->total_count > 0;
}

auto silent_decline_join_request(
    TelegramBotClient &bot,
    TelegramId chat_id,
    TelegramId user_id,
    std::string_view reason
) -> void {
    (void)bot.decline_chat_join_request(DeclineChatJoinRequest{
        .chat_id = chat_id,
        .user_id = user_id,
    });
    Log::Info("Join request silently declined: chat={}, user={}, reason={}", chat_id, user_id, reason);
}

auto silent_ban_member(
    TelegramBotClient &bot,
    TelegramId chat_id,
    TelegramId user_id,
    std::string_view reason
) -> void {
    (void)bot.ban_chat_member(BanChatMemberRequest{
        .chat_id = chat_id,
        .user_id = user_id,
        .until_date = k_permanent_ban_until_date,
        .revoke_messages = true,
    });
    Log::Info("Member silently banned: chat={}, user={}, reason={}", chat_id, user_id, reason);
}

auto profile_screen_and_block(
    TelegramBotClient &bot,
    TelegramId chat_id,
    const User &user,
    std::optional<std::string_view> bio,
    bool in_group
) -> bool {
    const auto result = ProfileScreen::evaluate(ProfileScreenInput{
        .user = user,
        .bio = bio,
        .has_avatar = has_user_avatar(bot, user.id),
    });
    if (!result.blocked) {
        return false;
    }
    if (in_group) {
        silent_ban_member(bot, chat_id, user.id, result.reason);
    } else {
        silent_decline_join_request(bot, chat_id, user.id, result.reason);
    }
    return true;
}

auto send_self_deleting_status(TelegramBotClient &bot, TelegramId chat_id, std::string text) -> void {
    const auto status = bot.send_message(SendMessageRequest{
        .chat_id = chat_id,
        .text = std::move(text),
        .reply_markup = ReplyKeyboardRemove{},
    });
    if (status.succeeded()) {
        delete_message_quietly(bot, chat_id, status.result->message_id);
    }
}

auto reject_timed_out_request(TelegramBotClient &bot, const PendingJoinRequest &pending) -> void {
    if (pending.test_only) {
        return;
    }
    if (pending.needs_unmute_on_success) {
        const auto current = bot.get_chat_member(GetChatMemberRequest{
            .chat_id = pending.chat_id,
            .user_id = pending.user_id,
        });
        if (current.succeeded() && current.result->status == "kicked") {
            return;
        }
        if (current.succeeded() && current.result->status == "left") {
            return;
        }
        (void)bot.ban_chat_member(BanChatMemberRequest{
            .chat_id = pending.chat_id,
            .user_id = pending.user_id,
            .until_date = k_permanent_ban_until_date,
            .revoke_messages = false,
        });
        (void)bot.unban_chat_member(UnbanChatMemberRequest{
            .chat_id = pending.chat_id,
            .user_id = pending.user_id,
            .only_if_banned = true,
        });
    } else {
        (void)bot.decline_chat_join_request(DeclineChatJoinRequest{
            .chat_id = pending.chat_id,
            .user_id = pending.user_id,
        });
    }
}

}  // namespace

VerificationService::VerificationService(
    std::string bot_username,
    MarkupFactory markup,
    PendingRepository &pending
)
    : bot_username_{std::move(bot_username)}
    , markup_{std::move(markup)}
    , pending_{pending} {}

auto VerificationService::handle_chat_join_request(
    TelegramBotClient &bot,
    const ChatJoinRequest &join_request
) -> void {
    pending_.record_user(join_request.from);

    if (join_request.from.is_bot) {
        return;
    }

    if (profile_screen_and_block(
        bot,
        join_request.chat.id,
        join_request.from,
        join_request.bio,
        false
    )) {
        return;
    }

    Log::Info(
        "Join request received: chat={}, user={}, user_chat_id={}",
        join_request.chat.id,
        join_request.from.id,
        join_request.user_chat_id
    );

    SendMessageRequest request{
        .chat_id = join_request.user_chat_id,
        .text = build_join_request_prompt(),
    };
    if (const auto markup = markup_.inline_guide_markup()) {
        request.reply_markup = *markup;
    }

    const auto prompt = bot.send_message(request);
    if (!prompt.succeeded()) {
        Log::Error(
            "Send join request prompt failed: user_chat_id={}, error={}",
            join_request.user_chat_id,
            prompt.description.value_or("unknown error")
        );
        return;
    }

    pending_.save(PendingJoinRequest{
        .chat_id = join_request.chat.id,
        .user_id = join_request.from.id,
        .user_chat_id = join_request.user_chat_id,
        .prompt_chat_id = join_request.user_chat_id,
        .prompt_message_id = prompt.result->message_id,
        .expires_at_millis = now_millis() + k_verification_timeout_millis,
    });

    Log::Info(
        "Join request pending: chat={}, user={}, prompt_message={}",
        join_request.chat.id,
        join_request.from.id,
        prompt.result->message_id
    );
}

auto VerificationService::begin_group_verification(
    TelegramBotClient &bot,
    TelegramId chat_id,
    const User &member,
    std::optional<std::int64_t> message_thread_id
) -> void {
    pending_.record_user(member);
    if (member.is_bot || pending_.consume_approved(member.id)) {
        return;
    }
    if (pending_.find_by_user_id(member.id).has_value()) {
        Log::Info("Verification already pending: chat={}, user={}", chat_id, member.id);
        return;
    }

    if (profile_screen_and_block(bot, chat_id, member, std::nullopt, true)) {
        return;
    }

    const auto restrict = bot.restrict_chat_member(RestrictChatMemberRequest{
        .chat_id = chat_id,
        .user_id = member.id,
        .permissions = muted_permissions(),
        .until_date = 0,
    });
    if (!restrict.succeeded()) {
        Log::Warn(
            "Restrict new member failed: chat={}, user={}, error={}",
            chat_id,
            member.id,
            restrict.description.value_or("unknown error")
        );
        return;
    }

    SendMessageRequest request{
        .chat_id = chat_id,
        .text = build_group_prompt(member),
        .parse_mode = std::string{"HTML"},
        .message_thread_id = message_thread_id,
    };
    if (const auto markup = markup_.inline_guide_markup(member.id)) {
        request.reply_markup = *markup;
    }

    const auto prompt = bot.send_message(request);
    if (!prompt.succeeded()) {
        Log::Error(
            "Send verification prompt failed: chat={}, user={}, error={}",
            chat_id,
            member.id,
            prompt.description.value_or("unknown error")
        );
        return;
    }

    pending_.save(PendingJoinRequest{
        .chat_id = chat_id,
        .user_id = member.id,
        .user_chat_id = member.id,
        .prompt_chat_id = chat_id,
        .prompt_message_id = prompt.result->message_id,
        .expires_at_millis = now_millis() + k_verification_timeout_millis,
        .needs_unmute_on_success = true,
    });

    Log::Info("New member muted for verification: chat={}, user={}", chat_id, member.id);
}

auto VerificationService::handle_new_chat_members(
    TelegramBotClient &bot,
    const Message &message
) -> void {
    if (!message.new_chat_members.has_value() || message.new_chat_members->empty()) {
        return;
    }

    delete_message_quietly(bot, message.chat.id, message.message_id);

    for (const auto &member : *message.new_chat_members) {
        begin_group_verification(bot, message.chat.id, member, message.message_thread_id);
    }
}

auto VerificationService::handle_chat_member(
    TelegramBotClient &bot,
    const ChatMemberUpdated &update
) -> void {
    if (!is_in_chat(update.old_chat_member) && is_in_chat(update.new_chat_member)) {
        begin_group_verification(bot, update.chat.id, update.new_chat_member.user, {});
    }
}

auto VerificationService::handle_left_chat_member(
    TelegramBotClient &bot,
    const Message &message
) -> void {
    delete_message_quietly(bot, message.chat.id, message.message_id);
}

auto VerificationService::handle_callback_query(
    TelegramBotClient &bot,
    const CallbackQuery &query
) -> void {
    if (!query.data.has_value() || !query.message.has_value()) {
        return;
    }

    const auto &data = *query.data;
    const auto &message = *query.message;

    if (data.starts_with("admin_pass:")) {
        const auto target_user_id = parse_callback_target_user_id(data, "admin_pass:");
        if (!target_user_id.has_value()) {
            (void)bot.answer_callback_query(AnswerCallbackQueryRequest{
                .callback_query_id = query.id,
                .text = std::string{"无效的用户 ID"},
            });
            return;
        }

        const auto operator_user_id = query.from.id;
        if (!can_restrict_members(bot, message.chat.id, operator_user_id)) {
            (void)bot.answer_callback_query(AnswerCallbackQueryRequest{
                .callback_query_id = query.id,
                .text = std::string{"只有拥有封禁/限制权限的管理员才能通过。"},
                .show_alert = true,
            });
            return;
        }

        (void)bot.answer_callback_query(AnswerCallbackQueryRequest{
            .callback_query_id = query.id,
            .text = std::string{"已通过该成员验证。"},
        });

        if (manual_approve_member(bot, message.chat.id, *target_user_id, operator_user_id)) {
            delete_message_quietly(bot, message.chat.id, message.message_id);
        }
        return;
    }

    if (data.starts_with("admin_ban:")) {
        const auto target_user_id = parse_callback_target_user_id(data, "admin_ban:");
        if (!target_user_id.has_value()) {
            (void)bot.answer_callback_query(AnswerCallbackQueryRequest{
                .callback_query_id = query.id,
                .text = std::string{"无效的用户 ID"},
            });
            return;
        }

        const auto operator_user_id = query.from.id;
        if (!can_restrict_members(bot, message.chat.id, operator_user_id)) {
            (void)bot.answer_callback_query(AnswerCallbackQueryRequest{
                .callback_query_id = query.id,
                .text = std::string{"只有拥有封禁权限的管理员才能封禁成员。"},
                .show_alert = true,
            });
            return;
        }

        if (!manual_ban_member(bot, message.chat.id, *target_user_id, operator_user_id)) {
            (void)bot.answer_callback_query(AnswerCallbackQueryRequest{
                .callback_query_id = query.id,
                .text = std::string{"封禁失败"},
                .show_alert = true,
            });
            return;
        }

        (void)bot.answer_callback_query(AnswerCallbackQueryRequest{
            .callback_query_id = query.id,
            .text = std::string{"已封禁该成员。"},
        });
        delete_message_quietly(bot, message.chat.id, message.message_id);
    }
}

auto VerificationService::handle_web_app_data(
    TelegramBotClient &bot,
    const Message &message
) -> void {
    if (!message.from.has_value() || !message.web_app_data.has_value()) {
        return;
    }

    VerificationPayload payload;
    try {
        payload = VerificationPayload::from_json_string(message.web_app_data->data);
    } catch (const std::exception &ex) {
        Log::Warn(
            "Invalid web_app_data from userId={}: {}",
            message.from->id,
            ex.what()
        );
        return;
    }

    if (!payload.is_verified_event()) {
        Log::Warn(
            "Unexpected web_app_data event from userId={}: {}",
            message.from->id,
            payload.event.has_value() ? *payload.event : payload.type.value_or("unknown")
        );
        return;
    }

    const auto user_id = message.from->id;
    const auto pending = pending_.find_by_user_id(user_id);
    if (!pending.has_value()) {
        delete_message_quietly(bot, message.chat.id, message.message_id);
        send_self_deleting_status(bot, message.chat.id, "没有找到待审批的入群申请。");
        return;
    }

    if (pending->expires_at_millis.has_value()
        && *pending->expires_at_millis <= now_millis()) {
        pending_.remove_by_user_id(user_id);
        cleanup_prompt_messages(bot, *pending);
        delete_message_quietly(bot, message.chat.id, message.message_id);
        reject_timed_out_request(bot, *pending);
        send_self_deleting_status(bot, message.chat.id, "验证已超时，请重新申请加入后重试。");
        Log::Info(
            "Expired verification rejected: userId={}, testOnly={}",
            pending->user_id,
            pending->test_only
        );
        return;
    }

    if (pending->needs_unmute_on_success) {
        const auto current = bot.get_chat_member(GetChatMemberRequest{
            .chat_id = pending->chat_id,
            .user_id = pending->user_id,
        });
        if (current.succeeded() && current.result->status == "kicked") {
            Log::Warn(
                "User was banned during verification: chat={}, user={}",
                pending->chat_id,
                pending->user_id
            );
            pending_.remove_by_user_id(user_id);
            cleanup_prompt_messages(bot, *pending);
            delete_message_quietly(bot, message.chat.id, message.message_id);
            send_self_deleting_status(bot, message.chat.id, "您已被管理员封禁，无法解除限制。");
            return;
        }

        (void)bot.restrict_chat_member(RestrictChatMemberRequest{
            .chat_id = pending->chat_id,
            .user_id = pending->user_id,
            .permissions = unmuted_permissions(),
            .until_date = 0,
        });
        Log::Info(
            "New member verification passed: chat={}, user={}, github={}",
            pending->chat_id,
            pending->user_id,
            payload.github_username.value_or("")
        );
    } else if (!pending->test_only) {
        (void)bot.approve_chat_join_request(ApproveChatJoinRequest{
            .chat_id = pending->chat_id,
            .user_id = pending->user_id,
        });
        pending_.mark_approved(pending->user_id);
        Log::Info(
            "Join request approved: chat={}, user={}, github={}",
            pending->chat_id,
            pending->user_id,
            payload.github_username.value_or("")
        );
    } else {
        Log::Info(
            "Verification test passed: chat={}, user={}, github={}",
            pending->chat_id,
            pending->user_id,
            payload.github_username.value_or("")
        );
    }

    pending_.remove_by_user_id(user_id);
    cleanup_prompt_messages(bot, *pending);
    delete_message_quietly(bot, message.chat.id, message.message_id);

    std::string status_text;
    if (pending->test_only) {
        status_text = "验证测试通过。";
    } else if (pending->needs_unmute_on_success) {
        status_text = "验证通过，已解除群内发言限制。";
    } else {
        status_text = "验证通过，已同意入群。";
    }

    const auto status = bot.send_message(SendMessageRequest{
        .chat_id = message.chat.id,
        .text = status_text,
        .reply_markup = ReplyKeyboardRemove{},
    });
    if (pending->needs_unmute_on_success && status.succeeded()) {
        delete_message_quietly(bot, message.chat.id, status.result->message_id);
    }

    Log::Info(
        "Verification completed: userId={}, github={}, testOnly={}",
        user_id,
        payload.github_username.value_or(""),
        pending->test_only
    );
}

auto VerificationService::expire_pending_verifications(TelegramBotClient &bot) -> void {
    const auto expired = pending_.remove_expired(now_millis());
    for (const auto &pending : expired) {
        cleanup_prompt_messages(bot, pending);

        if (pending.needs_unmute_on_success) {
            const auto current = bot.get_chat_member(GetChatMemberRequest{
                .chat_id = pending.chat_id,
                .user_id = pending.user_id,
            });
            if (current.succeeded() && current.result->status == "kicked") {
                Log::Info(
                    "Skip timeout kick, user already banned: chat={}, user={}",
                    pending.chat_id,
                    pending.user_id
                );
                continue;
            }
            if (current.succeeded() && current.result->status == "left") {
                continue;
            }

            (void)bot.ban_chat_member(BanChatMemberRequest{
                .chat_id = pending.chat_id,
                .user_id = pending.user_id,
                .until_date = k_permanent_ban_until_date,
                .revoke_messages = false,
            });
            (void)bot.unban_chat_member(UnbanChatMemberRequest{
                .chat_id = pending.chat_id,
                .user_id = pending.user_id,
                .only_if_banned = true,
            });
            Log::Info(
                "Verification timed out and user kicked: chat={}, user={}",
                pending.chat_id,
                pending.user_id
            );
        } else {
            (void)bot.decline_chat_join_request(DeclineChatJoinRequest{
                .chat_id = pending.chat_id,
                .user_id = pending.user_id,
            });
            (void)bot.send_message(
                pending.user_chat_id,
                "验证超时，入群申请已被拒绝。如需加入请重新发起申请。"
            );
            Log::Info(
                "Verification timed out and join request declined: chat={}, user={}",
                pending.chat_id,
                pending.user_id
            );
        }
    }
}

auto VerificationService::manual_approve_member(
    TelegramBotClient &bot,
    TelegramId chat_id,
    TelegramId target_user_id,
    std::optional<TelegramId> admin_id
) -> bool {
    const auto current = bot.get_chat_member(GetChatMemberRequest{
        .chat_id = chat_id,
        .user_id = target_user_id,
    });
    if (current.succeeded() && current.result->status == "kicked") {
        Log::Warn(
            "Cannot approve kicked user: chat={}, user={}, admin={}",
            chat_id,
            target_user_id,
            admin_id.value_or(0)
        );
        return false;
    }

    const auto result = bot.restrict_chat_member(RestrictChatMemberRequest{
        .chat_id = chat_id,
        .user_id = target_user_id,
        .permissions = unmuted_permissions(),
        .until_date = 0,
    });
    if (!result.succeeded()) {
        return false;
    }

    if (const auto pending = pending_.find_by_user_id(target_user_id)) {
        if (!pending->needs_unmute_on_success && !pending->test_only) {
            (void)bot.approve_chat_join_request(ApproveChatJoinRequest{
                .chat_id = pending->chat_id,
                .user_id = target_user_id,
            });
            pending_.mark_approved(target_user_id);
        }
        cleanup_prompt_messages(bot, *pending);
        pending_.remove_by_user_id(target_user_id);
    }

    Log::Info(
        "Member manually approved: chat={}, user={}, admin={}",
        chat_id,
        target_user_id,
        admin_id.value_or(0)
    );
    return true;
}

auto VerificationService::manual_ban_member(
    TelegramBotClient &bot,
    TelegramId chat_id,
    TelegramId target_user_id,
    std::optional<TelegramId> admin_id
) -> bool {
    const auto current = bot.get_chat_member(GetChatMemberRequest{
        .chat_id = chat_id,
        .user_id = target_user_id,
    });

    if (!current.succeeded() || current.result->status != "kicked") {
        const auto result = bot.ban_chat_member(BanChatMemberRequest{
            .chat_id = chat_id,
            .user_id = target_user_id,
            .until_date = k_permanent_ban_until_date,
            .revoke_messages = true,
        });
        if (!result.succeeded()) {
            return false;
        }
    }

    if (const auto pending = pending_.find_by_user_id(target_user_id)) {
        cleanup_prompt_messages(bot, *pending);
        pending_.remove_by_user_id(target_user_id);
    }

    Log::Info(
        "Member manually banned: chat={}, user={}, admin={}",
        chat_id,
        target_user_id,
        admin_id.value_or(0)
    );
    return true;
}
