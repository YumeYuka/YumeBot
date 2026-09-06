export module event.profile_screen;

import std;
import telegram.user;

export struct ProfileScreenInput {
    const User &user;
    std::optional<std::string_view> bio{};
    bool has_avatar{true};
};

export struct ProfileScreenResult {
    bool blocked{false};
    std::string reason{};
};

export class ProfileScreen {
public:
    [[nodiscard]] static auto evaluate(const ProfileScreenInput &input) -> ProfileScreenResult;
};
