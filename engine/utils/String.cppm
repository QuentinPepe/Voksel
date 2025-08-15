module;
#include <string>
#include <charconv>
#include <array>
#include <locale>
#include <type_traits>

export module Utils.String;

import Core.Types;
import Core.Assert;

export namespace Utils {
    template<typename T>
    concept IntegralButNotBool = std::is_integral_v<T> && !std::is_same_v<T, bool>;

    std::string ToString(IntegralButNotBool auto v) {
        std::array<char, 32> buf{};
        auto [ptr, ec]{std::to_chars(buf.data(), buf.data() + buf.size(), v)};
        assert(ec == std::errc{}, "to_chars failed");
        return std::string{buf.data(), static_cast<std::size_t>(ptr - buf.data())};
    }

    inline std::string ToString(F32 v, int precision = 6) {
        std::array<char, 64> buf{};
        auto [ptr, ec]{
            std::to_chars(buf.data(), buf.data() + buf.size(),
                          static_cast<double>(v), std::chars_format::general, precision)
        };
        assert(ec == std::errc{}, "to_chars failed");
        return std::string{buf.data(), static_cast<std::size_t>(ptr - buf.data())};
    }

    inline std::string ToString(F64 v, int precision = 6) {
        std::array<char, 64> buf{};
        auto [ptr, ec]{
            std::to_chars(buf.data(), buf.data() + buf.size(),
                          v, std::chars_format::general, precision)
        };
        assert(ec == std::errc{}, "to_chars failed");
        return std::string{buf.data(), static_cast<std::size_t>(ptr - buf.data())};
    }

    inline std::string ToString(bool v) {
        return v ? std::string{"true"} : std::string{"false"};
    }

    template<typename T>
        requires (!IntegralButNotBool<T> && !std::is_floating_point_v<T> && !std::is_same_v<T, bool>)
    std::string ToString(const T &v) {
        std::ostringstream oss{};
        oss.imbue(std::locale::classic());
        oss << v;
        auto s{oss.str()};
        assert(!s.empty(), "ostream conversion failed");
        return s;
    }
}
