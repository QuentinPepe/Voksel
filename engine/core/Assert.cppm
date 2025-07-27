export module Core.Assert;

import Core.Log;
import std;

export struct SourceLocation {
    const char* file_name;
    const char* function_name;
    int line;
    int column;
};

export void assert_fail(const char* expr, const SourceLocation& loc) {
    Logger::Critical("Assertion failed: {} in {} at {}:{}:{}",
                     expr, loc.function_name, loc.file_name, loc.line, loc.column);

#if defined(_MSC_VER)
    __debugbreak();
#endif

   std::exit(1);
}

export inline void assert(bool condition, const char* expr,
                         const SourceLocation& loc = {__builtin_FILE(), __builtin_FUNCTION(), __builtin_LINE(), __builtin_COLUMN()}) {
    if (!condition) {
        assert_fail(expr, loc);
    }
}