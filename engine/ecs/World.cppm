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
    static constexpr auto Components = std::tuple<Args...>{};
};

template<typename World, typename... Args>
struct QueryTraits<void(Query<World, Args...>, F32)> {
    using WorldType = World;
    using QueryType = Query<World, Args...>;
    static constexpr auto Components = std::tuple<Args...>{};
};

export class World {
private:
    struct ISystem {
        virtual ~ISystem() = default;
        virtual void Update(World* world, F32 dt) = 0;
    };

    template<typename Func>
    class System : public ISystem {
    private:
        Func m_Func;

    public:
        explicit System(Func func) : m_Func{func} {}

        void Update(World *world, F32 dt) override {
            using Traits = QueryTraits<Func>;
            typename Traits::QueryType query{world};
            m_Func(query, dt);
        }
    };

    EntityManager m_EntityManager;
    UnorderedMap<ComponentID, UniquePtr<IComponentStorageBase>> m_Storages;
    Vector<std::pair<std::string, UniquePtr<ISystem>>> m_Systems;

public:
    EntityHandle CreateEntity() {
        return m_EntityManager.CreateEntity();
    }

    void DestroyEntity(EntityHandle handle) {
        for (auto &storage: m_Storages | std::views::values) {
            if (storage->Contains(handle.id)) {
                // TODO
            }
        }
        m_EntityManager.DestroyEntity(handle);
    }

    template<typename T>
    void AddComponent(EntityHandle handle, T&& component){
        assert(m_EntityManager.IsValid(handle), "The handle is invalid.");
        auto componentID = ComponentRegistry::GetID<T>();
        if (!m_Storages.contains(componentID)) {
            m_Storages[componentID] = std::make_unique<ComponentStorage<T>>();
        }
        static_cast<ComponentStorage<T>*>(m_Storages[componentID].get())->Insert(handle.id, handle.generation, std::forward<T>(component));
    }

    template<typename T>
    void AddComponent(EntityID entity, T&& component) {
        if (auto generation = m_EntityManager.GetGeneration(entity); generation != INVALID_GENERATION) {
            AddComponent(EntityHandle{entity, generation}, std::forward<T>(component));
        }
    }

    // TODO RemoveComponent

    template<typename T>
    T* GetComponent(EntityHandle handle) {
        if (!m_EntityManager.IsValid(handle)) {
            return nullptr;
        }

        auto componentId = ComponentRegistry::GetID<T>();
        if (auto it = m_Storages.find(componentId); it != m_Storages.end()) {
            return static_cast<ComponentStorage<T>*>(it->second.get())->Get(handle.id, handle.generation);
        }
        return nullptr;
    }

    template<typename T>
    T* GetComponent(EntityID entity) {
        if (auto generation = m_EntityManager.GetGeneration(entity); generation != INVALID_GENERATION) {
            return GetComponent<T>(EntityHandle{entity, generation});
        }
        return nullptr;
    }

    template<typename T>
    ComponentStorage<T>* GetStorage() {
        auto componentId = ComponentRegistry::GetID<T>();
        if (auto it = m_Storages.find(componentId); it != m_Storages.end()) {
            return static_cast<ComponentStorage<T>*>(it->second.get());
        }
        return nullptr;
    }

    [[nodiscard]] EntityID GetMaxEntityId() const {
        return m_EntityManager.GetMaxEntityID();
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
        std::erase_if(m_Systems, [&name](const auto& pair) { return pair.first == name; });
    }

    [[nodiscard]] USize GetSystemCount() const {
        return m_Systems.size();
    }

    void Clear() {
        m_Storages.clear();
        m_Systems.clear();
        m_EntityManager.Clear();
    }
};
