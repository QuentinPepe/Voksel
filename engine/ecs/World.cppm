export module ECS.World;

import ECS.Component;
import ECS.Query;
import Core.Types;
import Core.Log;
import Core.Assert;
import std;

export {
    struct Position {
        float x, y, z;
    };

    struct Velocity {
        float dx, dy, dz;
    };

    struct Health {
        int hp;
        int max_hp;
    };

    struct Player {};
    struct Enemy {};
    struct Dead {};
}

export class World {
public:
    using SystemFunc = std::function<void(F32)>;

    EntityID CreateEntity() {
        return m_NextEntity++;
    }

    template<typename T>
    void AddComponent(EntityID entity, T component) {
        GetStorage<T>()->Insert(entity, 0, std::move(component));
    }

    template<typename T>
    ComponentStorage<T>* GetStorage();

    EntityID GetMaxEntityId() const {
        return m_NextEntity;
    }

    template<typename... Components>
    void AddSystem(std::string_view , void(*system)(Query<Components...>, F32)) {
        m_Systems.emplace_back([this, system](F32 dt) {

            auto query = MakeQuery<World, Components...>(this);
            system(query, dt);
        });
    }

    void Update(F32 dt) {
        for (auto& sys : m_Systems) {
            sys(dt);
        }
    }

private:
    ComponentStorage<Position> m_Positions;
    ComponentStorage<Velocity> m_Velocities;

    EntityID m_NextEntity = 0;
    std::vector<SystemFunc> m_Systems;
};

export template<>
ComponentStorage<Position>* World::GetStorage<Position>() {
    return &m_Positions;
}

export template<>
ComponentStorage<Velocity>* World::GetStorage<Velocity>() {
    return &m_Velocities;
}