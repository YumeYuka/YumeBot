module;

#include <ctime>

module log;

import std;

auto Log::SetLevel(const LogLevel level) noexcept -> void {
    level_ = level;
}

namespace {

auto GetLocalTime() -> std::tm {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);

    std::tm local{};

#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif

    return local;
}

}  // namespace

auto Log::LogMessage(const LogLevel level, std::string_view message) -> void {
    const auto local = GetLocalTime();

    std::println(
        "[{:02}-{:02} {:02}:{:02}:{:02}] ({}) {}",
        local.tm_mon + 1,
        local.tm_mday,
        local.tm_hour,
        local.tm_min,
        local.tm_sec,
        level,
        message
    );
    std::cout.flush();
}
