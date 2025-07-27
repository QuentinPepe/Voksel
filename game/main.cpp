import ECS.Query;
import ECS.Component;
import ECS.World;
import Core.Types;
import Core.Log;
import std;

struct Position {
    F32 x, y, z;
};

struct Velocity {
    F32 dx, dy, dz;
};

struct Health {
    S32 hp;
    S32 max_hp;
};

struct Player {};
struct Enemy {};
struct Dead {};

void MovementSystem(Query<World, Position, Velocity> query, F32 dt) {
    for (const auto& [pos, vel] : query.Iter<Position, Velocity>()) {
        pos->x += vel->dx * dt;
        pos->y += vel->dy * dt;
        pos->z += vel->dz * dt;
    }
}

void HealthSystem(Query<World, Health, Without<Dead>> query, F32) {
    for (const auto& [health] : query.Iter<Health>()) {
        if (health->hp <= 0) {
            Logger::Warn("Entity died!");
        }
    }
}

void PlayerMovementSystem(Query<World, Position, Velocity, With<Player>> query, F32 dt) {
    for (const auto& [pos, vel] : query.Iter<Position, Velocity>()) {
        pos->x += vel->dx * dt * 2.0f;
        pos->y += vel->dy * dt * 2.0f;
        pos->z += vel->dz * dt * 2.0f;
    }
}

void EnemyAISystem(Query<World, Position, With<Enemy>, Without<Dead>> query, F32 dt) {
    for (const auto& [pos] : query.Iter<Position>()) {
        pos->x += std::sin(pos->y) * dt;
        pos->y += std::cos(pos->x) * dt;
    }
}

void RenderSystem(Query<World, Position, Optional<Health>> query, F32) {
    static int frame = 0;
    if (frame++ % 60 == 0) {
        Logger::Info("=== Render Frame ===");
        for (const auto& [pos, health] : query.Iter<Position, Health>()) {
            if (health) {
                Logger::Info("Entity at ({}, {}) with {} HP", pos->x, pos->y, health->hp);
            } else {
                Logger::Info("Entity at ({}, {}) no health", pos->x, pos->y);
            }
        }
    }
}

void ComplexQueryTest() {
    World world;

    const auto e1 = world.CreateEntity();
    world.AddComponent(e1, Position{1.0f, 1.0f, 1.0f});
    world.AddComponent(e1, Velocity{0.1f, 0.1f, 0.1f});
    world.AddComponent(e1, Health{100, 100});

    const auto e2 = world.CreateEntity();
    world.AddComponent(e2, Position{2.0f, 2.0f, 2.0f});
    world.AddComponent(e2, Velocity{0.2f, 0.2f, 0.2f});

    const auto e3 = world.CreateEntity();
    world.AddComponent(e3, Position{3.0f, 3.0f, 3.0f});
    world.AddComponent(e3, Health{50, 50});

    {
        Query<World, Position, Velocity> q1(&world);
        Logger::Info("Entities with Position AND Velocity: {}", q1.Count());
        for (const auto& [pos, vel] : q1.Iter<Position, Velocity>()) {
            Logger::Info("  Pos: {}, Vel: {}", pos->x, vel->dx);
        }
    }

    {
        Query<World, Position, With<Health>> q2(&world);
        Logger::Info("Entities with Position that also have Health: {}", q2.Count());
        for (const auto& [pos] : q2.Iter<Position>()) {
            Logger::Info("  Pos: {}", pos->x);
        }
    }

    {
        Query<World, Position, Without<Velocity>> q3(&world);
        Logger::Info("Entities with Position but WITHOUT Velocity: {}", q3.Count());
        for (const auto& [pos] : q3.Iter<Position>()) {
            Logger::Info("  Pos: {}", pos->x);
        }
    }

    {
        Query<World, Position, Optional<Health>> q4(&world);
        Logger::Info("All entities with Position (Health optional): {}", q4.Count());
        for (const auto& [pos, health] : q4.Iter<Position, Health>()) {
            if (health) {
                Logger::Info("  Pos: {}, Health: {}", pos->x, health->hp);
            } else {
                Logger::Info("  Pos: {}, No health", pos->x);
            }
        }
    }
}

int main() {
    Logger::SetLevel(LogLevel::Debug);
    Logger::SetFile("voxel_engine.log");

    Logger::Info("=== Complex Query Test ===");
    ComplexQueryTest();

    Logger::Info("\n=== World System Test ===");
    World world;

    world.AddSystem("Movement", MovementSystem);
    world.AddSystem("Health", HealthSystem);
    world.AddSystem("PlayerMovement", PlayerMovementSystem);
    world.AddSystem("EnemyAI", EnemyAISystem);
    world.AddSystem("Render", RenderSystem);

    const auto player = world.CreateEntity();
    world.AddComponent(player, Position{0.0f, 0.0f, 0.0f});
    world.AddComponent(player, Velocity{10.0f, 0.0f, 0.0f});
    world.AddComponent(player, Health{100, 100});
    world.AddComponent(player, Player{});

    for (int i = 0; i < 5; ++i) {
        const auto enemy = world.CreateEntity();
        world.AddComponent(enemy, Position{float(i * 20), 50.0f, 0.0f});
        world.AddComponent(enemy, Health{50, 50});
        world.AddComponent(enemy, Enemy{});
    }

    const auto prop = world.CreateEntity();
    world.AddComponent(prop, Position{100.0f, 100.0f, 0.0f});

    const F32 dt = 1.0f / 60.0f;
    for (int i = 0; i < 120; ++i) {
        world.Update(dt);
    }

    Logger::Info("World Stats:");
    Logger::Info("  Entities: {}", world.GetMaxEntityId());
    Logger::Info("  Positions: {}", world.GetStorage<Position>()->Size());
    Logger::Info("  Velocities: {}", world.GetStorage<Velocity>()->Size());
    Logger::Info("  Healths: {}", world.GetStorage<Health>()->Size());
    Logger::Info("  Players: {}", world.GetStorage<Player>()->Size());
    Logger::Info("  Enemies: {}", world.GetStorage<Enemy>()->Size());

    return 0;
}
