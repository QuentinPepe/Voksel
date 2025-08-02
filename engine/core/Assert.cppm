export module Core.Assert;

import Core.Log;
import std;

export struct Callsite {
    const char* file_name;
    const char* function_name;
    int line;
    int column;

    static consteval Callsite current(
        const char* file = __builtin_FILE(),
        const char* function = __builtin_FUNCTION(),
        int line = __builtin_LINE(),
        int column = __builtin_COLUMN()) noexcept {
        return {file, function, line, column};
    }
};

export void assert_fail(const char* expr, const Callsite& loc) {
    Logger::Critical(LogSystem, "Assertion failed: {} in {} at {}:{}:{}",
                     expr, loc.function_name, loc.file_name, loc.line, loc.column);

#if defined(_MSC_VER)
    __debugbreak();
#elif defined(__GNUC__) || defined(__clang__)
    __builtin_trap();
#endif

    std::exit(1);
}

export constexpr void assert(bool condition, const char* expr,
                            const Callsite& loc = Callsite::current()) {
#ifndef NDEBUG
    if (std::is_constant_evaluated()) {
        if (!condition) {
            struct assertion_failed_at_compile_time {};
            auto trigger_error = assertion_failed_at_compile_time{};
        }
    } else {
        if (!condition) [[unlikely]] {
            assert_fail(expr, loc);
        }
    }
#else

    (void)condition;
    (void)expr;
    (void)loc;
#endif
}