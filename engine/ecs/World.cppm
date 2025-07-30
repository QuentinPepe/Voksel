export module ECS.World;

import ECS.Component;
import ECS.Query;
import Core.Types;
import Core.Assert;
import std;

template<typename T> struct QueryTraits;

template<typename World, typename... Args>
struct QueryTraits<void(*)(Query<World, Args...>, F32)> {
    using WorldType = World;
    using QueryType = Query<World, Args...>;
    static constexpr Archetype includeMask = QueryType::GetIncludeMask();
    static constexpr Archetype excludeMask = QueryType::GetExcludeMask();
};

template<typename World, typename... Args>
struct QueryTraits<void(Query<World, Args...>, F32)> {
    using WorldType = World;
    using QueryType = Query<World, Args...>;
    static constexpr Archetype includeMask = QueryType::GetIncludeMask();
    static constexpr Archetype excludeMask = QueryType::GetExcludeMask();
};

export class World {
private:

    struct IComponentStorageBase {
        virtual ~IComponentStorageBase() = default;
        [[nodiscard]] virtual bool Contains(EntityHandle handle) const = 0;
        [[nodiscard]] virtual void* GetRaw(EntityHandle handle) = 0;
        [[nodiscard]] virtual const void* GetRaw(EntityHandle handle) const = 0;
        virtual void Remove(EntityHandle handle) = 0;
        virtual void Clear() = 0;
        [[nodiscard]] virtual USize Size() const = 0;
    };

    template<typename T>
    class TypedStorage : public IComponentStorageBase {
        ComponentStorage<T> storage;

    public:
        [[nodiscard]] bool Contains(EntityHandle handle) const override {
            return storage.Contains(handle);
        }

        [[nodiscard]] void* GetRaw(EntityHandle handle) override {
            return storage.Get(handle);
        }

        [[nodiscard]] const void* GetRaw(EntityHandle handle) const override {
            return storage.Get(handle);
        }

        void Remove(EntityHandle handle) override {
            storage.Remove(handle);
        }

        void Clear() override {
            storage.Clear();
        }

        [[nodiscard]] USize Size() const override {
            return storage.Size();
        }

        ComponentStorage<T>* GetStorage() { return &storage; }
    };

    struct ISystem {
        virtual ~ISystem() = default;
        virtual void Update(World* world, F32 dt) = 0;
        [[nodiscard]] virtual Archetype GetIncludeMask() const = 0;
        [[nodiscard]] virtual Archetype GetExcludeMask() const = 0;
    };

    template<typename Func>
    class System : public ISystem {
        Func m_Func;

    public:
        explicit System(Func func) : m_Func{func} {}

        void Update(World* world, F32 dt) override {
            using Traits = QueryTraits<Func>;
            typename Traits::QueryType query{world};
            m_Func(query, dt);
        }

        [[nodiscard]] Archetype GetIncludeMask() const override {
            using Traits = QueryTraits<Func>;
            return Traits::includeMask;
        }

        [[nodiscard]] Archetype GetExcludeMask() const override {
            using Traits = QueryTraits<Func>;
            return Traits::excludeMask;
        }
    };

    EntityManager m_EntityManager{};
    UnorderedMap<ComponentID, UniquePtr<IComponentStorageBase>> m_Storages{};
    Vector<std::pair<std::string, UniquePtr<ISystem>>> m_Systems{};

    UnorderedMap<EntityHandle, Archetype> m_EntityArchetypes{};

    bool m_ArchetypeCacheDirty{false};

    void UpdateEntityArchetype(EntityHandle handle) {
        Archetype arch = 0;

        for (const auto& [compId, storage] : m_Storages) {
            if (storage->Contains(handle)) {
                arch |= (1ULL << compId);
            }
        }

        m_EntityArchetypes[handle] = arch;
    }

public:
    using EntityArchetypePair = std::pair<EntityHandle, Archetype>;
    using EntityIterator = UnorderedMap<EntityHandle, Archetype>::const_iterator;

    [[nodiscard]] EntityIterator EntitiesBegin() const {
        return m_EntityArchetypes.begin();
    }

    [[nodiscard]] EntityIterator EntitiesEnd() const {
        return m_EntityArchetypes.end();
    }

    EntityHandle CreateEntity() {
        const auto handle = m_EntityManager.CreateEntity();
        if (handle.valid()) {
            m_EntityArchetypes[handle] = EMPTY_ARCHETYPE;
        }
        return handle;
    }

    void DestroyEntity(EntityHandle handle) {
        if (!handle.valid()) return;

        for (const auto &storage: m_Storages | std::views::values) {
            if (storage->Contains(handle)) {
                storage->Remove(handle);
            }
        }

        m_EntityArchetypes.erase(handle);
        m_EntityManager.DestroyEntity(handle);
    }

    template<typename T>
    void AddComponent(EntityHandle handle, T&& component) {
        assert(handle.valid(), "Invalid entity handle");

        ComponentID componentID = ComponentRegistry::GetID<T>();

        if (!m_Storages.contains(componentID)) {
            m_Storages[componentID] = std::make_unique<TypedStorage<T>>();
        }

        auto* typedStorage = static_cast<TypedStorage<T>*>(m_Storages[componentID].get());
        typedStorage->GetStorage()->Insert(handle, std::forward<T>(component));

        m_EntityArchetypes[handle] |= (1ULL << componentID);
    }

    template<typename T>
    void RemoveComponent(EntityHandle handle) {
        if (!handle.valid()) return;

        ComponentID componentID = ComponentRegistry::GetID<T>();
        auto it = m_Storages.find(componentID);
        if (it == m_Storages.end()) return;

        it->second->Remove(handle);

        m_EntityArchetypes[handle] &= ~(1ULL << componentID);
    }

    template<typename T>
    [[nodiscard]] T* GetComponent(EntityHandle handle) {
        if (!handle.valid()) return nullptr;

        ComponentID componentID = ComponentRegistry::GetID<T>();
        auto it = m_Storages.find(componentID);
        if (it == m_Storages.end()) return nullptr;

        return static_cast<T*>(it->second->GetRaw(handle));
    }

    template<typename T>
    [[nodiscard]] const T* GetComponent(EntityHandle handle) const {
        return const_cast<World*>(this)->GetComponent<T>(handle);
    }

    template<typename T>
    [[nodiscard]] ComponentStorage<T>* GetStorage() {
        ComponentID componentID = ComponentRegistry::GetID<T>();
        auto it = m_Storages.find(componentID);
        if (it == m_Storages.end()) return nullptr;

        return static_cast<TypedStorage<T>*>(it->second.get())->GetStorage();
    }

    [[nodiscard]] Archetype GetEntityArchetype(EntityHandle handle) const {
        auto it = m_EntityArchetypes.find(handle);
        return it != m_EntityArchetypes.end() ? it->second : EMPTY_ARCHETYPE;
    }

    template<typename Func>
    void AddSystem(const std::string& name, Func func) {
        m_Systems.emplace_back(name, std::make_unique<System<Func>>(func));
    }

    void Update(F32 dt) {

        for (const auto &system: m_Systems | std::views::values) {
            system->Update(this, dt);
        }
    }

    void RemoveSystem(const std::string& name) {
        std::erase_if(m_Systems, [&name](const auto& pair) {
            return pair.first == name;
        });
    }

    [[nodiscard]] USize GetSystemCount() const {
        return m_Systems.size();
    }

    [[nodiscard]] U16 GetMaxEntityId() const {
        return m_EntityManager.GetMaxEntityID();
    }

    void Clear() {
        m_Storages.clear();
        m_Systems.clear();
        m_EntityArchetypes.clear();
        m_EntityManager.Clear();
    }

    [[nodiscard]] USize GetEntityCount() const {
        return m_EntityArchetypes.size();
    }

    [[nodiscard]] USize GetComponentTypeCount() const {
        return m_Storages.size();
    }
};