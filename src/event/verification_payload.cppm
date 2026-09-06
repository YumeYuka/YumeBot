export module event.verification_payload;

import std;
import telegram.json;

export struct VerificationPayload {
    std::optional<int> schema_version{};
    std::optional<std::string> event{};
    std::optional<std::string> type{};
    std::optional<std::string> github_username{};
    std::optional<int> quiz_score{};

    [[nodiscard]] auto is_verified_event() const -> bool;

    static auto from_json_string(std::string_view data) -> VerificationPayload;
};
