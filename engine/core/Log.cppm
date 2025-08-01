export module Core.Log;

import Core.Types;
import std;

export enum class LogLevel : U8 {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Critical = 5,
};

export struct LogCategory {
    const char* name;
    constexpr explicit LogCategory(const char* n) : name{n} {}
};

export template<typename Tag>
struct CategoryWithTag : LogCategory {
    using tag_type = Tag;
    constexpr explicit CategoryWithTag(const char* n) : LogCategory{n} {}
};

export struct GeneralTag {};
export inline constexpr CategoryWithTag<GeneralTag> LogGeneral{"General"};

export struct SystemTag {};
export inline constexpr CategoryWithTag<SystemTag> LogSystem{"System"};

export struct GraphicsTag {};
export inline constexpr CategoryWithTag<GraphicsTag> LogGraphics{"Graphics"};

export struct ECSTag {};
export inline constexpr CategoryWithTag<ECSTag> LogECS{"ECS"};

export struct InputTag {};
export inline constexpr CategoryWithTag<InputTag> LogInput{"Input"};

export struct TasksTag {};
export inline constexpr CategoryWithTag<TasksTag> LogTasks{"Tasks"};

export struct GameTag {};
export inline constexpr CategoryWithTag<GameTag> LogGame{"Game"};

export template<LogLevel MinLevel = LogLevel::Info>
struct LogConfig {
    static constexpr LogLevel min_level = MinLevel;
};

export template<typename Category>
struct CategoryEnabled : std::true_type {};

/* Example :
template<>
struct CategoryEnabled<GeneralTag> : std::false_type {};
*/

template<typename T>
concept IsLogCategory = std::is_base_of_v<LogCategory, std::remove_cvref_t<T>>;

export template<typename Config = LogConfig<>>
class LoggerBase {
private:
    static inline std::mutex s_Mutex;
    static inline std::ofstream s_FileOutput;
    static inline bool s_ColorEnabled = true;

    template<LogLevel Level, typename Category, typename... Args>
    static void LogImpl(const Category& category, std::string_view format, Args &&... args) {
        if constexpr (Level < Config::min_level) {
            return;
        }

        if constexpr (requires { typename Category::tag_type; }) {
            if constexpr (!CategoryEnabled<typename Category::tag_type>::value) {
                return;
            }
        }

        auto now = std::chrono::system_clock::now();
        std::string message = std::vformat(format, std::make_format_args<std::format_context>(args...));

        std::lock_guard lock{s_Mutex};

        if (s_FileOutput.is_open()) {
            s_FileOutput << std::format("[{:%H:%M:%S}] [{}] [{}] {}\n",
                std::chrono::floor<std::chrono::seconds>(now),
                LevelToString(Level),
                category.name,
                message);
        }

        if (s_ColorEnabled) {
            std::cout << std::format("{}[{:%H:%M:%S}] [{}] [{}] {}\033[0m\n",
                GetLevelColor(Level),
                std::chrono::floor<std::chrono::seconds>(now),
                LevelToString(Level),
                category.name,
                message);
        } else {
            std::cout << std::format("[{:%H:%M:%S}] [{}] [{}] {}\n",
                std::chrono::floor<std::chrono::seconds>(now),
                LevelToString(Level),
                category.name,
                message);
        }
    }

    static constexpr const char *GetLevelColor(LogLevel level) {
        switch (level) {
            case LogLevel::Trace: return "\033[90m";
            case LogLevel::Debug: return "\033[36m";
            case LogLevel::Info: return "\033[32m";
            case LogLevel::Warning: return "\033[33m";
            case LogLevel::Error: return "\033[31m";
            case LogLevel::Critical: return "\033[35m";
        }
        return "\033[0m";
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
    static void SetFile(const std::filesystem::path &path) {
        std::lock_guard lock{s_Mutex};
        s_FileOutput.open(path);
    }

    static void EnableColor(bool enable) {
        s_ColorEnabled = enable;
    }

    template<typename Category = decltype(LogGeneral), typename... Args>
        requires IsLogCategory<Category>
    static void Trace(const Category& category, std::string_view format, Args &&... args) {
        LogImpl<LogLevel::Trace>(category, format, std::forward<Args>(args)...);
    }

    template<typename Category = decltype(LogGeneral), typename... Args>
        requires IsLogCategory<Category>
    static void Debug(const Category& category, std::string_view format, Args &&... args) {
        LogImpl<LogLevel::Debug>(category, format, std::forward<Args>(args)...);
    }

    template<typename Category = decltype(LogGeneral), typename... Args>
        requires IsLogCategory<Category>
    static void Info(const Category& category,std::string_view format, Args &&... args) {
        LogImpl<LogLevel::Info>(category, format, std::forward<Args>(args)...);
    }

    template<typename Category = decltype(LogGeneral), typename... Args>
        requires IsLogCategory<Category>
    static void Warn(const Category& category, std::string_view format, Args &&... args) {
        LogImpl<LogLevel::Warning>(category, format, std::forward<Args>(args)...);
    }

    template<typename Category = decltype(LogGeneral), typename... Args>
        requires IsLogCategory<Category>
    static void Error(const Category& category, std::string_view format, Args &&... args) {
        LogImpl<LogLevel::Error>(category, format, std::forward<Args>(args)...);
    }

    template<typename Category = decltype(LogGeneral), typename... Args>
        requires IsLogCategory<Category>
    static void Critical(const Category& category, std::string_view format, Args &&... args) {
        LogImpl<LogLevel::Critical>(category, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Trace(std::string_view format, Args &&... args) {
        Trace(LogGeneral, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Debug(std::string_view format, Args &&... args) {
        Debug(LogGeneral, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Info(std::string_view format, Args &&... args) {
        Info(LogGeneral, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Warn(std::string_view format, Args &&... args) {
        Warn(LogGeneral, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Error(std::string_view format, Args &&... args) {
        Error(LogGeneral, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Critical(std::string_view format, Args &&... args) {
        Critical(LogGeneral, format, std::forward<Args>(args)...);
    }
};

export using ReleaseLogger = LoggerBase<LogConfig<LogLevel::Info>>;
export using DebugLogger = LoggerBase<LogConfig<LogLevel::Debug>>;
export using TraceLogger = LoggerBase<LogConfig<LogLevel::Trace>>;

#ifdef NDEBUG
    export using Logger = ReleaseLogger;
#else
    export using Logger = DebugLogger;
#endif