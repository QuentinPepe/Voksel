export module Core.Types;

import std;

export {
    using U8 = std::uint8_t;
    using U16 = std::uint16_t;
    using U32 = std::uint32_t;
    using U64 = std::uint64_t;

    using S8 = std::int8_t;
    using S16 = std::int16_t;
    using S32 = std::int32_t;
    using S64 = std::int64_t;

    using F32 = float;
    using F64 = double;

    using USize = std::size_t;
    using SSize = std::ptrdiff_t;

    using EntityID = U32;
    using ComponentID = U16;
    using ArchetypeID = U32;

    inline constexpr USize CACHE_LINE_SIZE = 64;

    inline constexpr EntityID INVALID_ENTITY = std::numeric_limits<EntityID>::max();
}
