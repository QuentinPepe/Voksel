import ECS.Query;
import ECS.Component;
import Core.Types;
import Core.Log;
import Core.Assert;
import std;

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

class TestWorld {
private:
    ComponentStorage<Position> m_Positions;
    ComponentStorage<Velocity> m_Velocities;
    ComponentStorage<Health> m_Healths;
    ComponentStorage<Player> m_Players;
    ComponentStorage<Enemy> m_Enemies;
    ComponentStorage<Dead> m_Dead;
    EntityID m_NextEntity = 0;

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

    void PrintStats() {
        Logger::Info("World Stats:");
        Logger::Info("  Entities: {}", m_NextEntity);
        Logger::Info("  Positions: {}", m_Positions.Size());
        Logger::Info("  Velocities: {}", m_Velocities.Size());
        Logger::Info("  Healths: {}", m_Healths.Size());
        Logger::Info("  Players: {}", m_Players.Size());
        Logger::Info("  Enemies: {}", m_Enemies.Size());
        Logger::Info("  Dead: {}", m_Dead.Size());
    }
};

void TestBasicQuery() {
    Logger::Info("=== Test Basic Query ===");

    TestWorld world;

    auto e1 = world.CreateEntity();
    world.AddComponent(e1, Position{1.0f, 2.0f, 3.0f});

    auto e2 = world.CreateEntity();
    world.AddComponent(e2, Position{4.0f, 5.0f, 6.0f});

    world.CreateEntity();

    auto query = MakeQuery<TestWorld, Position>(&world);

    Logger::Info("Entities with Position:");
    int count = 0;
    for (auto [pos] : query) {
        Logger::Info("  Entity at ({}, {}, {})", pos.x, pos.y, pos.z);
        count++;
    }

    assert(count == 2, "count == 2");
    assert(query.Count() == 2, "query.Count() == 2");
    assert(!query.IsEmpty(), "!query.IsEmpty()");

    Logger::Debug("✓ Basic query passed");
}

void TestMultipleComponents() {
    Logger::Info("=== Test Multiple Components ===");

    TestWorld world;

    auto e1 = world.CreateEntity();
    world.AddComponent(e1, Position{10.0f, 0.0f, 0.0f});
    world.AddComponent(e1, Velocity{1.0f, 0.0f, 0.0f});

    auto e2 = world.CreateEntity();
    world.AddComponent(e2, Position{20.0f, 0.0f, 0.0f});

    auto e3 = world.CreateEntity();
    world.AddComponent(e3, Velocity{2.0f, 0.0f, 0.0f});

    auto query = MakeQuery<TestWorld, Position, Velocity>(&world);

    Logger::Info("Entities with Position AND Velocity:");
    int count = 0;
    for (auto [pos, vel] : query) {
        Logger::Info("  Entity at {} moving at {}", pos.x, vel.dx);
        count++;
    }

    assert(count == 1, "count == 1");
    Logger::Debug("✓ Multiple components query passed");
}

void TestFilteredQuery() {
    Logger::Info("=== Test Filtered Query ===");

    TestWorld world;

    auto player = world.CreateEntity();
    world.AddComponent(player, Position{100.0f, 50.0f, 0.0f});
    world.AddComponent(player, Health{100, 100});
    world.AddComponent(player, Player{});

    auto enemy1 = world.CreateEntity();
    world.AddComponent(enemy1, Position{200.0f, 50.0f, 0.0f});
    world.AddComponent(enemy1, Health{50, 50});
    world.AddComponent(enemy1, Enemy{});

    auto enemy2 = world.CreateEntity();
    world.AddComponent(enemy2, Position{300.0f, 50.0f, 0.0f});
    world.AddComponent(enemy2, Health{0, 50});
    world.AddComponent(enemy2, Enemy{});
    world.AddComponent(enemy2, Dead{});

    QueryBuilder<TestWorld> builder(&world);
    auto aliveEnemies = builder.With<Enemy>().Without<Dead>().Build<Position, Health>();

    Logger::Info("Alive enemies:");
    int count = 0;
    for (auto [pos, health] : aliveEnemies) {
        Logger::Info("  Enemy at {} with {} HP", pos.x, health.hp);
        count++;
    }

    assert(count == 1, "count == 1");
    Logger::Debug("✓ Filtered query passed");
}

void TestQueryMethods() {
    Logger::Info("=== Test Query Methods ===");

    TestWorld world;

    auto e1 = world.CreateEntity();
    world.AddComponent(e1, Position{1.0f, 1.0f, 1.0f});
    world.AddComponent(e1, Velocity{0.1f, 0.1f, 0.1f});

    auto e2 = world.CreateEntity();
    world.AddComponent(e2, Position{2.0f, 2.0f, 2.0f});
    world.AddComponent(e2, Velocity{0.2f, 0.2f, 0.2f});

    auto query = MakeQuery<TestWorld, Position, Velocity>(&world);

    // Test Get avec entity existante
    if (auto result = query.Get(e1)) {
        auto [pos, vel] = *result;
        Logger::Info("Entity {} found at {}", e1, pos.x);
        assert(pos.x == 1.0f, "pos.x == 1.0f");
    }

    // Test Get avec entity non-existante
    if (auto result = query.Get(999)) {
        assert(false, "Should not find non-existent entity");
    } else {
        Logger::Info("Entity 999 not found (expected)");
    }

    Logger::Info("ForEach test:");
    query.ForEach([](Position& pos, Velocity& vel) {
        Logger::Debug("  Processing entity at {}", pos.x);
        pos.x += vel.dx;
    });

    auto result = query.Get(e1);
    assert(result.has_value(), "result.has_value()");
    auto [pos, vel] = *result;
    assert(pos.x == 1.1f, "pos.x == 1.1f");

    Logger::Debug("✓ Query methods passed");
}

void TestPerformance() {
    Logger::Info("=== Test Performance ===");

    TestWorld world;
    const int ENTITY_COUNT = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto e = world.CreateEntity();
        world.AddComponent(e, Position{float(i), 0.0f, 0.0f});

        if (i % 2 == 0) {
            world.AddComponent(e, Velocity{1.0f, 0.0f, 0.0f});
        }

        if (i % 5 == 0) {
            world.AddComponent(e, Health{100, 100});
        }

        if (i % 100 == 0) {
            world.AddComponent(e, Player{});
        }
    }

    auto creation_end = std::chrono::high_resolution_clock::now();
    auto creation_ms = std::chrono::duration<double, std::milli>(creation_end - start).count();

    Logger::Info("Created {} entities in {}ms", ENTITY_COUNT, creation_ms);

    auto query_start = std::chrono::high_resolution_clock::now();
    auto query = MakeQuery<TestWorld, Position, Velocity>(&world);
    int count = 0;

    for (auto [pos, vel] : query) {
        pos.x += vel.dx;
        count++;
    }

    auto query_end = std::chrono::high_resolution_clock::now();
    auto query_ms = std::chrono::duration<double, std::milli>(query_end - query_start).count();

    Logger::Info("Processed {} entities in {}ms", count, query_ms);
    Logger::Info("Average: {}ns per entity", (query_ms / count * 1000000));

    Logger::Debug("✓ Performance test completed");
}

void TestParallelExecution() {
    Logger::Info("=== Test Parallel Execution ===");

    TestWorld world;
    const int ENTITY_COUNT = 10000;

    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto e = world.CreateEntity();
        world.AddComponent(e, Position{float(i), 0.0f, 0.0f});
        world.AddComponent(e, Velocity{1.0f, 0.0f, 0.0f});
    }

    auto query = MakeQuery<TestWorld, Position, Velocity>(&world);

    auto seq_start = std::chrono::high_resolution_clock::now();
    query.ForEach([](Position& pos, Velocity& vel) {
        pos.x += vel.dx;
        for (int i = 0; i < 1000; ++i) {
            pos.y = std::sin(pos.x) * std::cos(pos.x);
        }
    });
    auto seq_end = std::chrono::high_resolution_clock::now();
    auto seq_ms = std::chrono::duration<double, std::milli>(seq_end - seq_start).count();

    auto par_start = std::chrono::high_resolution_clock::now();
    query.ParForEach([](Position& pos, Velocity& vel) {
        pos.z += vel.dx;
        for (int i = 0; i < 1000; ++i) {
            pos.y = std::sin(pos.z) * std::cos(pos.z);
        }
    });
    auto par_end = std::chrono::high_resolution_clock::now();
    auto par_ms = std::chrono::duration<double, std::milli>(par_end - par_start).count();

    Logger::Info("Sequential: {}ms", seq_ms);
    Logger::Info("Parallel: {}ms", par_ms);
    Logger::Info("Speedup: {}x", (seq_ms / par_ms));

    Logger::Debug("✓ Parallel execution test completed");
}

int main() {
    Logger::SetLevel(LogLevel::Debug);
    Logger::SetFile("voxel_engine.log");

    Logger::Info("Starting the Voksel Engine v{}.{}.{}", 0, 1, 0);

    Logger::Info("Starting ECS Query Tests");
    Logger::Info("========================");

    try {
        TestBasicQuery();
        TestMultipleComponents();
        TestFilteredQuery();
        TestQueryMethods();
        TestPerformance();
        TestParallelExecution();

        Logger::Info("✅ ALL TESTS PASSED!");
    } catch (const std::exception& e) {
        Logger::Error("❌ Test failed: {}", e.what());
        return 1;
    }

    return 0;
}
