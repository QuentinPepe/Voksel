export module ECS.SystemScheduler;

import ECS.World;
import ECS.Query;
import ECS.Component;
import Core.Types;
import Core.Assert;
import Core.Log;
import std;

// Forward declarations
export class SystemScheduler;

export enum class SystemStage : U8 {
    PreUpdate,
    Update,
    PostUpdate,
    PreRender,
    Render,
    PostRender
};

export enum class SystemPriority : S8 {
    Critical = 127,
    High = 64,
    Normal = 0,
    Low = -64,
    Idle = -128
};

export struct SystemDependency {
    enum Type : U8 {
        Before,
        After,
        With
    } type;

    std::string targetSystem;
};

export class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void Configure(SystemScheduler& scheduler) = 0;
    virtual void Run(World* world, F32 dt) = 0;
    [[nodiscard]] virtual std::string GetName() const = 0;
    [[nodiscard]] virtual SystemStage GetStage() const { return SystemStage::Update; }
};

export struct SystemMetadata {
    std::string name;
    SystemStage stage;
    Vector<SystemDependency> dependencies;
    Archetype readComponents{0};
    Archetype writeComponents{0};
    bool isParallel{true};
    SystemPriority priority{SystemPriority::Normal};
};

export struct SystemNode {
    ISystem* system;
    SystemMetadata metadata;
    Vector<SystemNode*> dependencies;
    Vector<SystemNode*> dependents;
    U32 nodeId;
};

export struct SystemExecutionStats {
    Vector<std::pair<std::string, U64>> systemTimes;
};

export template<typename Derived>
class System : public ISystem {
protected:
    SystemMetadata m_Metadata{};

    void SetName(std::string name) { m_Metadata.name = std::move(name); }
    void SetStage(SystemStage stage) { m_Metadata.stage = stage; }
    void SetPriority(SystemPriority priority) { m_Metadata.priority = priority; }
    void SetParallel(bool parallel) { m_Metadata.isParallel = parallel; }

    void RunBefore(const std::string& system) {
        m_Metadata.dependencies.push_back({SystemDependency::Before, system});
    }

    void RunAfter(const std::string& system) {
        m_Metadata.dependencies.push_back({SystemDependency::After, system});
    }

    void RunWith(const std::string& system) {
        m_Metadata.dependencies.push_back({SystemDependency::With, system});
    }

    template<typename... Components>
    void ReadsComponents() {
        m_Metadata.readComponents |= MakeArchetype<Components...>();
    }

    template<typename... Components>
    void WritesComponents() {
        m_Metadata.writeComponents |= MakeArchetype<Components...>();
    }

public:
    void Configure(SystemScheduler& scheduler) override;

    [[nodiscard]] std::string GetName() const override { return m_Metadata.name; }
    [[nodiscard]] SystemStage GetStage() const override { return m_Metadata.stage; }
    [[nodiscard]] const SystemMetadata& GetMetadata() const { return m_Metadata; }
};

export template<typename Derived, typename... QueryArgs>
class QuerySystem : public System<Derived> {
private:
    using QueryType = Query<World, QueryArgs...>;

protected:
    void Setup() {
        constexpr Archetype readMask = QueryType::GetReadMask();
        constexpr Archetype writeMask = QueryType::GetWriteMask();

        this->m_Metadata.readComponents = readMask;
        this->m_Metadata.writeComponents = writeMask;
    }

    void ForEach(World *world, auto func) {
        QueryType query{world};
        query.ForEach(func);
    }

    QueryType GetQuery(World* world) {
        return QueryType{world};
    }
};

export class SystemScheduler {
private:
    Vector<std::unique_ptr<ISystem>> m_Systems;
    UnorderedMap<std::string, SystemNode*> m_SystemNodes;
    UnorderedMap<SystemStage, Vector<SystemNode*>> m_StageNodes;
    Vector<std::unique_ptr<SystemNode>> m_NodeStorage;

    World* m_World{nullptr};
    UnorderedMap<std::string, U64> m_SystemExecutionTimes;
    std::function<void(SystemNode*)> m_ExecuteCallback;

public:
    SystemScheduler() = default;

    template<typename T, typename... Args>
    T* AddSystem(Args&&... args);

    void RegisterSystemMetadata(ISystem* system, const SystemMetadata& metadata);
    void BuildExecutionGraph(World* world);
    void SetExecuteCallback(std::function<void(SystemNode*)> callback);
    void ExecuteSystem(SystemNode* node, F32 dt);

    [[nodiscard]] const UnorderedMap<SystemStage, Vector<SystemNode*>>& GetStageNodes() const {
        return m_StageNodes;
    }

    [[nodiscard]] const UnorderedMap<std::string, SystemNode*>& GetSystemNodes() const {
        return m_SystemNodes;
    }

    [[nodiscard]] std::string GenerateDotGraph() const;
    [[nodiscard]] SystemExecutionStats GetStats() const;

    static const char* GetStageName(SystemStage stage);

private:
    void ResolveExplicitDependencies();
    void InferComponentDependencies();
    void CheckComponentConflict(SystemNode* a, SystemNode* b);
    bool HasExplicitRelationship(SystemNode* a, SystemNode* b) const;
    bool IsImplicitDependency(const SystemNode* dependent, const SystemNode* dependency) const;
};

// Template implementations
template<typename Derived>
void System<Derived>::Configure(SystemScheduler& scheduler) {
    static_cast<Derived*>(this)->Setup();
    scheduler.RegisterSystemMetadata(this, m_Metadata);
}

template<typename T, typename... Args>
T* SystemScheduler::AddSystem(Args&&... args) {
    auto system = std::make_unique<T>(std::forward<Args>(args)...);
    T* ptr = system.get();

    system->Configure(*this);

    m_Systems.push_back(std::move(system));
    return ptr;
}