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

    constexpr EntityID INVALID_ENTITY = std::numeric_limits<EntityID>::max();
    constexpr U32 INVALID_INDEX = std::numeric_limits<U32>::max();
    constexpr U32 INVALID_GENERATION = std::numeric_limits<U32>::max();

    template<class _Ty, class _Alloc = std::allocator<_Ty>>
    using Vector = std::vector<_Ty, _Alloc>;

    template <class _Ty, size_t _Size>
    using Array = std::array<_Ty, _Size>;
}
