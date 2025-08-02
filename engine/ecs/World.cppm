export module ECS.World;

import ECS.Component;
import Core.Types;
import Core.Assert;
import std;

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

    EntityManager m_EntityManager{};
    UnorderedMap<ComponentID, UniquePtr<IComponentStorageBase>> m_Storages{};
    UnorderedMap<EntityHandle, Archetype> m_EntityArchetypes{};

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

    [[nodiscard]] U16 GetMaxEntityId() const {
        return m_EntityManager.GetMaxEntityID();
    }

    void Clear() {
        m_Storages.clear();
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