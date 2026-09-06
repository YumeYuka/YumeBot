export module log;

import std;

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
    static auto SetLevel(LogLevel level) noexcept -> void;

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

    static auto LogMessage(LogLevel level, std::string_view message) -> void;
};
