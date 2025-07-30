export module ECS.Component;

import Core.Types;
import std;

export struct EntityHandle {
    U32 packed{};

    constexpr EntityHandle() = default;
    constexpr EntityHandle(U16 id, U16 generation)
        : packed{(static_cast<U32>(generation) << 16) | id} {}

    [[nodiscard]] constexpr U16 id() const { return packed & 0xFFFF; }
    [[nodiscard]] constexpr U16 generation() const { return packed >> 16; }
    [[nodiscard]] constexpr bool valid() const { return packed != 0; }

    constexpr bool operator==(const EntityHandle&) const = default;
};

export template<>
struct std::hash<EntityHandle> {
    constexpr size_t operator()(EntityHandle const& h) const noexcept {
        return h.packed;
    }
};

static consteval U64 ConstexprFNV1a(const char* s) {
    U64 h = 14681457744538843682ULL;
    while (*s) {
        h ^= static_cast<U64>(*s++);
        h *= 1099511628211ULL;
    }
    return h;
}


export template <typename T>
struct ComponentTypeID {
    static consteval ComponentID value() {
        static_assert([]{return false;}(), "Specialize ComponentTypeID<T> for each component!");
        return 0;
    }
};

export class ComponentRegistry {
public:
    template<typename T>
    static consteval ComponentID GetID() {
        return ComponentTypeID<T>::value();
    }
};


export using Archetype = U64;

export constexpr Archetype EMPTY_ARCHETYPE = 0;

export template<typename... Components>
consteval Archetype MakeArchetype() {
    Archetype arch = 0;
    ((arch |= (1ULL << ComponentRegistry::GetID<Components>())), ...);
    return arch;
}

export template<typename T>
class ComponentStorage {
private:
    static constexpr U32 CHUNK_SIZE = 64;

    struct Chunk {
        alignas(64) std::array<T, CHUNK_SIZE> components{};
        std::array<EntityHandle, CHUNK_SIZE> entities{};
        U32 count{0};
    };

    Vector<UniquePtr<Chunk>> m_Chunks{};
    UnorderedMap<U16, std::pair<U32, U32>> m_EntityToChunk{};

public:
    void Insert(EntityHandle handle, T component) {
        auto [it, inserted] = m_EntityToChunk.try_emplace(handle.id());

        if (!inserted) {

            auto [chunkIdx, compIdx] = it->second;
            m_Chunks[chunkIdx]->components[compIdx] = std::move(component);
            m_Chunks[chunkIdx]->entities[compIdx] = handle;
            return;
        }

        for (U32 i = 0; i < m_Chunks.size(); ++i) {
            if (m_Chunks[i]->count < CHUNK_SIZE) {
                U32 idx = m_Chunks[i]->count++;
                m_Chunks[i]->components[idx] = std::move(component);
                m_Chunks[i]->entities[idx] = handle;
                it->second = {i, idx};
                return;
            }
        }

        auto chunk = std::make_unique<Chunk>();
        chunk->components[0] = std::move(component);
        chunk->entities[0] = handle;
        chunk->count = 1;
        it->second = {static_cast<U32>(m_Chunks.size()), 0};
        m_Chunks.push_back(std::move(chunk));
    }

    void Remove(EntityHandle handle) {
        auto it = m_EntityToChunk.find(handle.id());
        if (it == m_EntityToChunk.end()) return;

        auto [chunkIdx, compIdx] = it->second;
        auto& chunk = *m_Chunks[chunkIdx];

        U32 lastIdx = chunk.count - 1;
        if (compIdx != lastIdx) {
            chunk.components[compIdx] = std::move(chunk.components[lastIdx]);
            chunk.entities[compIdx] = chunk.entities[lastIdx];

            m_EntityToChunk[chunk.entities[compIdx].id()] = {chunkIdx, compIdx};
        }

        chunk.count--;
        m_EntityToChunk.erase(it);
    }

    [[nodiscard]] T* Get(EntityHandle handle) {
        auto it = m_EntityToChunk.find(handle.id());
        if (it == m_EntityToChunk.end()) return nullptr;

        auto [chunkIdx, compIdx] = it->second;
        auto& entity = m_Chunks[chunkIdx]->entities[compIdx];

        if (entity.generation() != handle.generation()) return nullptr;

        return &m_Chunks[chunkIdx]->components[compIdx];
    }

    [[nodiscard]] const T* Get(EntityHandle handle) const {
        return const_cast<ComponentStorage*>(this)->Get(handle);
    }

    [[nodiscard]] bool Contains(EntityHandle handle) const {
        auto it = m_EntityToChunk.find(handle.id());
        if (it == m_EntityToChunk.end()) return false;

        auto [chunkIdx, compIdx] = it->second;
        return m_Chunks[chunkIdx]->entities[compIdx].generation() == handle.generation();
    }

    class Iterator {
        const ComponentStorage* storage;
        U32 chunkIdx;
        U32 compIdx;

        void Advance() {
            compIdx++;
            while (chunkIdx < storage->m_Chunks.size() &&
                   compIdx >= storage->m_Chunks[chunkIdx]->count) {
                chunkIdx++;
                compIdx = 0;
            }
        }

    public:
        Iterator(const ComponentStorage* s, U32 ci, U32 ii)
            : storage{s}, chunkIdx{ci}, compIdx{ii} {}

        Iterator& operator++() {
            Advance();
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return chunkIdx != other.chunkIdx || compIdx != other.compIdx;
        }

        std::pair<EntityHandle, T&> operator*() {
            auto& chunk = *storage->m_Chunks[chunkIdx];
            return {chunk.entities[compIdx],
                    const_cast<T&>(chunk.components[compIdx])};
        }
    };

    Iterator begin() const { return Iterator{this, 0, 0}; }
    Iterator end() const { return Iterator{this, static_cast<U32>(m_Chunks.size()), 0}; }

    void Clear() {
        m_Chunks.clear();
        m_EntityToChunk.clear();
    }

    [[nodiscard]] USize Size() const {
        USize total = 0;
        for (const auto& chunk : m_Chunks) {
            total += chunk->count;
        }
        return total;
    }
};

export class EntityManager {
private:
    static constexpr U16 MAX_ENTITIES = 0xFFFF;

    Vector<EntityHandle> m_FreeList{};
    U16 m_NextEntity{1};
    U16 m_NextGeneration{1};

public:
    [[nodiscard]] EntityHandle CreateEntity() {
        if (!m_FreeList.empty()) {
            auto handle = m_FreeList.back();
            m_FreeList.pop_back();

            U16 newGen = (handle.generation() + 1) & 0xFFFF;
            if (newGen == 0) newGen = 1;

            return EntityHandle{handle.id(), newGen};
        }

        if (m_NextEntity == MAX_ENTITIES) {
            return {};
        }

        return EntityHandle{m_NextEntity++, m_NextGeneration};
    }

    void DestroyEntity(EntityHandle handle) {
        if (!handle.valid()) return;
        m_FreeList.push_back(handle);
    }

    [[nodiscard]] constexpr U16 GetMaxEntityID() const {
        return m_NextEntity;
    }

    void Clear() {
        m_FreeList.clear();
        m_NextEntity = 1;
        m_NextGeneration = 1;
    }
};