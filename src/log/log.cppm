export module log;

import std;

#include <ctime>

export enum class LogLevel : int {
    Debug,
    Info,
    Warn,
    Error,
};


template<>
struct std::formatter<LogLevel> : std::formatter<std::string_view> {
    auto format(const LogLevel level, auto &ctx) const {
        std::string_view levelStr;
        switch (level) {
            case LogLevel::Debug: levelStr = "DEBUG";
                break;
            case LogLevel::Info: levelStr = "INFO";
                break;
            case LogLevel::Warn: levelStr = "WARN";
                break;
            case LogLevel::Error: levelStr = "ERROR";
                break;
        }
        return std::formatter<std::string_view>::format(levelStr, ctx);
    }
};

export class Log {
public:
    static auto SetLevel(const LogLevel level) noexcept -> void {
        level_ = level;
    }

    template<typename... Args>
    static auto Debug(std::format_string<Args...> fmt, Args &&... args) -> void {
        if (ShouldLog(LogLevel::Debug)) {
            LogMessage(LogLevel::Debug, std::format(fmt, std::forward<Args>(args)...));
        }
    }

    template<typename... Args>
    static auto Info(std::format_string<Args...> fmt, Args &&... args) -> void {
        if (ShouldLog(LogLevel::Info)) {
            LogMessage(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
        }
    }

    template<typename... Args>
    static auto Warn(std::format_string<Args...> fmt, Args &&... args) -> void {
        if (ShouldLog(LogLevel::Warn)) {
            LogMessage(LogLevel::Warn, std::format(fmt, std::forward<Args>(args)...));
        }
    }

    template<typename... Args>
    static auto Error(std::format_string<Args...> fmt, Args &&... args) -> void {
        if (ShouldLog(LogLevel::Error)) {
            LogMessage(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
        }
    }

private:
    inline static auto level_{LogLevel::Info};

    static auto ShouldLog(const LogLevel level) noexcept -> bool {
        return std::to_underlying(level) >= std::to_underlying(level_);
    }

    static auto GetLocalTime() -> std::tm {
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

    static auto LogMessage(const LogLevel level, std::string_view message) -> void {
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
    }
};
