export module ECS.Component;

import Core.Types;
import std;

export class IComponentStorageBase {
public:
    virtual ~IComponentStorageBase() = default;
    [[nodiscard]] virtual bool Contains(EntityID entity) const = 0;
    [[nodiscard]] virtual void* GetRaw(EntityID entity) = 0;
    [[nodiscard]] virtual const void* GetRaw(EntityID entity) const = 0;
    [[nodiscard]] virtual USize Size() const = 0;
    [[nodiscard]] virtual void Clear() = 0;
};

export template<typename T>
class ComponentStorage : public IComponentStorageBase {
private:
    struct EntityData {
        EntityID entity;
        U32 generation;
    };

    Vector<U32> m_Sparse{};
    Vector<EntityData> m_Entities{};
    Vector<T> m_Components{};
    Vector<U32> m_Generations{};

public:
    void Insert(EntityID entityID, U32 generation, T component) {
        if (entityID >= m_Sparse.size()) {
            m_Sparse.resize(entityID + 1, INVALID_INDEX);
            m_Generations.resize(entityID + 1, 0);
        }

        if (m_Sparse[entityID] != INVALID_INDEX) {
            if (m_Generations[entityID] == generation) {
                m_Components[m_Sparse[entityID]] = std::move(component);
            }
        } else {
            m_Sparse[entityID] = static_cast<U32>(m_Entities.size());
            m_Generations[entityID] = generation;
            m_Entities.push_back(EntityData{entityID, generation});
            m_Components.push_back(std::move(component));
        }
    }

    void Remove(EntityID entityID, U32 generation) {
        if (entityID >= m_Sparse.size() ||
            m_Sparse[entityID] == INVALID_INDEX ||
            m_Generations[entityID] != generation) {
            return;
        }

        U32 index = m_Sparse[entityID];
        U32 lastIndex = static_cast<U32>(m_Entities.size() - 1);

        if (index != lastIndex) {
            m_Entities[index] = m_Entities[lastIndex];
            m_Components[index] = std::move(m_Components[lastIndex]);
            m_Sparse[m_Entities[index].entity] = index;
        }

        m_Entities.pop_back();
        m_Components.pop_back();
        m_Sparse[entityID] = INVALID_INDEX;
    }

    [[nodiscard]] T* Get(EntityID entity, U32 generation) {
        if (entity >= m_Sparse.size() ||
            m_Sparse[entity] == INVALID_INDEX ||
            m_Generations[entity] != generation) {
            return nullptr;
        }
        return &m_Components[m_Sparse[entity]];
    }

    [[nodiscard]] const T* Get(EntityID entity, U32 generation) const {
        if (entity >= m_Sparse.size() ||
            m_Sparse[entity] == INVALID_INDEX ||
            m_Generations[entity] != generation) {
            return nullptr;
        }
        return &m_Components[m_Sparse[entity]];
    }

    [[nodiscard]] T* GetUnchecked(EntityID entity) {
        return &m_Components[m_Sparse[entity]];
    }

    [[nodiscard]] const T* GetUnchecked(EntityID entity) const {
        return &m_Components[m_Sparse[entity]];
    }

    [[nodiscard]] bool Contains(EntityID entityID) const override {
        return entityID < m_Sparse.size() && m_Sparse[entityID] != INVALID_INDEX;
    }

    [[nodiscard]] bool Contains(EntityID entityID, U32 generation) const {
        return entityID < m_Sparse.size() &&
               m_Sparse[entityID] != INVALID_INDEX &&
               m_Generations[entityID] == generation;
    }

    [[nodiscard]] void* GetRaw(EntityID entity) override {
        if (entity >= m_Sparse.size() || m_Sparse[entity] == INVALID_INDEX) {
            return nullptr;
        }
        return &m_Components[m_Sparse[entity]];
    }

    [[nodiscard]] const void* GetRaw(EntityID entity) const override {
        if (entity >= m_Sparse.size() || m_Sparse[entity] == INVALID_INDEX) {
            return nullptr;
        }
        return &m_Components[m_Sparse[entity]];
    }

    [[nodiscard]] USize Size() const override {
        return m_Components.size();
    }

    void Clear() override {
        m_Sparse.clear();
        m_Entities.clear();
        m_Components.clear();
        m_Generations.clear();
    }

    [[nodiscard]] auto begin() { return m_Components.begin(); }
    [[nodiscard]] auto end() { return m_Components.end(); }
    [[nodiscard]] auto begin() const { return m_Components.begin(); }
    [[nodiscard]] auto end() const { return m_Components.end(); }

    [[nodiscard]] const Vector<EntityData>& GetEntities() const { return m_Entities; }
    [[nodiscard]] const Vector<T>& GetComponents() const { return m_Components; }

    class Iterator {
    private:
        const ComponentStorage* m_Storage;
        USize m_Index;

    public:
        Iterator(const ComponentStorage* storage, USize index)
            : m_Storage{storage}, m_Index{index} {}

        bool operator!=(const Iterator& other) const {
            return m_Index != other.m_Index;
        }

        Iterator& operator++() {
            ++m_Index;
            return *this;
        }

        std::pair<EntityID, T&> operator*() {
            return std::pair<EntityID, T&>{
                m_Storage->m_Entities[m_Index].entity,
                const_cast<T&>(m_Storage->m_Components[m_Index])
            };
        }
    };

    Iterator IterateWith() { return Iterator{this, 0}; }
    Iterator IterateEnd() { return Iterator{this, m_Components.size()}; }
};

export class ComponentRegistry {
private:
    static inline std::atomic<ComponentID> s_NextID{0};

public:
    template<typename T>
    static ComponentID GetID() {
        static ComponentID id = s_NextID.fetch_add(1);
        return id;
    }
};

export struct EntityHandle {
    EntityID id;
    U32 generation;

    bool operator==(const EntityHandle& other) const {
        return id == other.id && generation == other.generation;
    }

    bool operator!=(const EntityHandle& other) const {
        return !(*this == other);
    }
};

export class EntityManager {
private:
    Vector<U32> m_Generations{};
    Vector<EntityID> m_FreeList{};
    EntityID m_NextEntity{0};

public:
    [[nodiscard]] EntityHandle CreateEntity() {
        EntityID id;
        U32 generation;

        if (!m_FreeList.empty()) {
            id = m_FreeList.back();
            m_FreeList.pop_back();
            generation = ++m_Generations[id];
        } else {
            id = m_NextEntity++;
            m_Generations.push_back(0);
            generation = 0;
        }

        return EntityHandle{id, generation};
    }

    void DestroyEntity(EntityHandle handle) {
        if (handle.id >= m_Generations.size() ||
            m_Generations[handle.id] != handle.generation) {
            return;
        }

        m_FreeList.push_back(handle.id);
    }

    [[nodiscard]] bool IsValid(EntityHandle handle) const {
        return handle.id < m_Generations.size() &&
               m_Generations[handle.id] == handle.generation;
    }

    [[nodiscard]] U32 GetGeneration(EntityID id) const {
        return id < m_Generations.size() ? m_Generations[id] : INVALID_GENERATION;
    }

    [[nodiscard]] EntityID GetMaxEntityID() const {
        return m_NextEntity;
    }

    void Clear() {
        m_Generations.clear();
        m_FreeList.clear();
        m_NextEntity = 0;
    }
};
