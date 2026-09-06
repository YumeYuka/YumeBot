module event.verification_payload;

import std;
import telegram.json;

namespace {

constexpr int k_verify_schema_version = 1;
constexpr int k_quiz_passing_score = 70;
constexpr int k_quiz_max_score = 100;

auto is_verify_event(std::string_view event) -> bool {
    return event == "github_join_request_verified"
        || event == "core_dev_links_verified"
        || event == "quiz_verified";
}

}  // namespace

auto VerificationPayload::is_verified_event() const -> bool {
    if (schema_version.has_value()
        && *schema_version == k_verify_schema_version
        && event.has_value()
        && is_verify_event(*event)) {
        if (*event == "quiz_verified") {
            return quiz_score.has_value()
                && *quiz_score >= k_quiz_passing_score
                && *quiz_score <= k_quiz_max_score;
        }
        return true;
    }
    return type.has_value() && *type == "github_join_verification";
}

auto VerificationPayload::from_json_string(std::string_view data) -> VerificationPayload {
    const auto json = JsonValue::parse(data);
    VerificationPayload payload;
    payload.schema_version = json_i64(json, "schemaVersion").transform(
        [](const std::int64_t value) { return static_cast<int>(value); }
    );
    payload.event = json_string(json, "event");
    payload.type = json_string(json, "type");
    payload.github_username = json_string(json, "githubUsername");
    if (const auto score = json_i64(json, "quizScore")) {
        payload.quiz_score = static_cast<int>(*score);
    }
    return payload;
}
