export module ECS.World;

import ECS.Component;
import ECS.Query;
import Core.Types;
import std;

export class World {
private:
    ComponentStorage<Position> m_Positions;
    ComponentStorage<Velocity> m_Velocities;
    ComponentStorage<Health> m_Healths;
    ComponentStorage<Player> m_Players;
    ComponentStorage<Enemy> m_Enemies;
    ComponentStorage<Dead> m_Dead;
    EntityID m_NextEntity = 0;

    struct SystemInfo {
        std::string name;
        std::function<void(F32)> func;
    };
    std::vector<SystemInfo> m_Systems;

public:
    EntityID CreateEntity() {
        return m_NextEntity++;
    }

    EntityID GetMaxEntityId() const {
        return m_NextEntity;
    }

    template<typename T>
    ComponentStorage<T>* GetStorage() {
        if constexpr (std::is_same_v<T, Position>) return &m_Positions;
        else if constexpr (std::is_same_v<T, Velocity>) return &m_Velocities;
        else if constexpr (std::is_same_v<T, Health>) return &m_Healths;
        else if constexpr (std::is_same_v<T, Player>) return &m_Players;
        else if constexpr (std::is_same_v<T, Enemy>) return &m_Enemies;
        else if constexpr (std::is_same_v<T, Dead>) return &m_Dead;
    }

    template<typename T>
    void AddComponent(EntityID entity, T component) {
        GetStorage<T>()->Insert(entity, 0, std::move(component));
    }

    template<typename T>
    void RemoveComponent(EntityID entity) {
        GetStorage<T>()->Remove(entity, 0);
    }

    template<typename... Args, typename Func>
    void AddSystem(const std::string& name, Func func) {
        m_Systems.push_back({name, [this, func](F32 dt) {
            Query<World, Args...> query(this);
            func(query, dt);
        }});
    }

    void Update(F32 dt) const {
        for (auto& system : m_Systems) {
            system.func(dt);
        }
    }

    void PrintStats() {
        std::cout << "World Stats:\n";
        std::cout << "  Entities: " << m_NextEntity << "\n";
        std::cout << "  Positions: " << m_Positions.Size() << "\n";
        std::cout << "  Velocities: " << m_Velocities.Size() << "\n";
        std::cout << "  Healths: " << m_Healths.Size() << "\n";
        std::cout << "  Players: " << m_Players.Size() << "\n";
        std::cout << "  Enemies: " << m_Enemies.Size() << "\n";
        std::cout << "  Dead: " << m_Dead.Size() << "\n";
    }
};
