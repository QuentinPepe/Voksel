export module Core.Log;

import Core.Types;
import std;

export enum class LogLevel : U8 {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
};

export class Logger {
private:
    static inline std::mutex s_Mutex;
    static inline LogLevel s_MinLogLevel = LogLevel::Info;
    static inline std::ofstream s_FileOutput;

    template<typename... Args>
    static void LogImpl(LogLevel level, std::string_view format, Args &&... args) {
        if (level < s_MinLogLevel) {
            return;
        }

        auto now = std::chrono::system_clock::now();

        std::string message = std::vformat(format, std::make_format_args<std::format_context>(args...));

        std::lock_guard lock{s_Mutex};

        // Format: [TIME] [LEVEL] Message
        auto &out = s_FileOutput.is_open() ? s_FileOutput : std::cout;
        out << std::format("[{:%H:%M:%S}] [{}] {}\n", std::chrono::floor<std::chrono::seconds>(now),
                           LevelToString(level), message);
    }

    static constexpr const char *LevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::Trace: return "TRACE";
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info: return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Critical: return "CRIT";
        }
        return "??????";
    }

public:
    static void SetLevel(LogLevel level) {
        s_MinLogLevel = level;
    }

    static void SetFile(const std::filesystem::path &path) {
        std::lock_guard lock{s_Mutex};
        s_FileOutput.open(path);
    }

    template<typename... Args>
    static void Trace(std::string_view format, Args &&... args) {
        LogImpl(LogLevel::Trace, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Debug(std::string_view format, Args &&... args) {
        LogImpl(LogLevel::Debug, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Info(std::string_view format, Args &&... args) {
        LogImpl(LogLevel::Info, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Warn(std::string_view format, Args &&... args) {
        LogImpl(LogLevel::Warning, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Error(std::string_view format, Args &&... args) {
        LogImpl(LogLevel::Error, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Critical(std::string_view format, Args &&... args) {
        LogImpl(LogLevel::Critical, format, std::forward<Args>(args)...);
    }
};