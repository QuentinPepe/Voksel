export module Tasks.ECSIntegration;

import ECS.SystemScheduler;
import ECS.World;
import Tasks.TaskGraph;
import Tasks.Orchestrator;
import Core.Types;
import Core.Assert;
import Core.Log;
import std;

// Adapter to integrate ECS with TaskGraph
export class ECSTaskAdapter {
private:
    SystemScheduler* m_Scheduler;
    EngineOrchestrator* m_Orchestrator;
    World* m_World;
    F32 m_DeltaTime{0.016f};

    // Map SystemPriority to TaskPriority
    static TaskPriority ConvertPriority(SystemPriority sysPriority) {
        switch (sysPriority) {
            case SystemPriority::Critical: return TaskPriority::Critical;
            case SystemPriority::High: return TaskPriority::High;
            case SystemPriority::Normal: return TaskPriority::Normal;
            case SystemPriority::Low: return TaskPriority::Low;
            case SystemPriority::Idle: return TaskPriority::Idle;
            default: return TaskPriority::Normal;
        }
    }

    static std::string GetStagePhaseName(SystemStage stage) {
        switch (stage) {
            case SystemStage::PreUpdate: return "PreUpdate";
            case SystemStage::Update: return "Update";
            case SystemStage::PostUpdate: return "PostUpdate";
            case SystemStage::PreRender: return "PreRender";
            case SystemStage::Render: return "Render";
            case SystemStage::PostRender: return "PostRender";
            default: return "Update";
        }
    }

public:
    ECSTaskAdapter(SystemScheduler* scheduler, EngineOrchestrator* orchestrator)
        : m_Scheduler{scheduler}, m_Orchestrator{orchestrator} {
        assert(scheduler, "SystemScheduler cannot be null");
        assert(orchestrator, "EngineOrchestrator cannot be null");
    }

    void BuildExecutionGraph(World* world) {
        m_World = world;
        m_Scheduler->BuildExecutionGraph(world);

        // Create tasks in the orchestrator for each system
        CreateOrchestratorTasks();

        Logger::Info(LogECS, "ECS systems integrated with TaskGraph");
    }

    void SetDeltaTime(F32 dt) {
        m_DeltaTime = dt;
    }

    void Execute() {
        // Execution is handled by the orchestrator
    }

private:
    void CreateOrchestratorTasks() {
        // Map to store task pointers for dependency resolution
        UnorderedMap<const SystemNode*, Task*> nodeToTask;

        // Create tasks for each system
        for (const auto& [stage, nodes] : m_Scheduler->GetStageNodes()) {
            std::string phaseName = GetStagePhaseName(stage);

            for (auto* node : nodes) {
                Task* task = m_Orchestrator->AddTaskToPhase(
                    phaseName,
                    node->metadata.name,
                    [this, node]() {
                        m_Scheduler->ExecuteSystem(node, m_DeltaTime);
                    },
                    ConvertPriority(node->metadata.priority)
                );

                nodeToTask[node] = task;
            }
        }

        // Add dependencies between tasks
        for (const auto& [stage, nodes] : m_Scheduler->GetStageNodes()) {
            for (auto* node : nodes) {
                Task* nodeTask = nodeToTask[node];
                if (!nodeTask) continue;

                for (auto* dep : node->dependencies) {
                    // Only add dependencies within the same stage
                    if (dep->metadata.stage == stage) {
                        Task* depTask = nodeToTask[dep];
                        if (depTask) {
                            nodeTask->AddDependency(depTask);
                        }
                    }
                }
            }
        }
    }
};

// Extension for EngineOrchestrator to support ECS
export class EngineOrchestratorECS {
private:
    EngineOrchestrator* m_Orchestrator;
    std::unique_ptr<SystemScheduler> m_SystemScheduler;
    std::unique_ptr<ECSTaskAdapter> m_ECSAdapter;

public:
    explicit EngineOrchestratorECS(EngineOrchestrator* orchestrator)
        : m_Orchestrator{orchestrator} {
        m_SystemScheduler = std::make_unique<SystemScheduler>();
        m_ECSAdapter = std::make_unique<ECSTaskAdapter>(m_SystemScheduler.get(), orchestrator);
    }

    SystemScheduler* GetSystemScheduler() {
        return m_SystemScheduler.get();
    }

    void BuildECSExecutionGraph(World* world) {
        m_ECSAdapter->BuildExecutionGraph(world);
    }

    void UpdateECS(F32 dt) {
        m_ECSAdapter->SetDeltaTime(dt);
        // Tasks are executed by the orchestrator automatically
    }
};