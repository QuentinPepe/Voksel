export module Tasks.ECSIntegration;

import ECS.SystemScheduler;
import ECS.World;
import Tasks.TaskGraph;
import Tasks.Orchestrator;
import Core.Types;
import Core.Assert;
import Core.Log;
import std;

export class ECSTaskAdapter {
private:
    SystemScheduler* m_Scheduler;
    EngineOrchestrator* m_Orchestrator;
    World* m_World;
    F32 m_DeltaTime{0.016f};

    // Add synchronization
    mutable std::mutex m_ExecutionMutex;
    std::atomic<bool> m_IsExecuting{false};

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
        std::lock_guard lock(m_ExecutionMutex);

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

        // Create tasks for each system with proper synchronization
        for (const auto& [stage, nodes] : m_Scheduler->GetStageNodes()) {
            std::string phaseName = GetStagePhaseName(stage);

            for (auto* node : nodes) {
                // Capture node by value to ensure it's valid during execution
                auto* capturedNode = node;
                auto* capturedScheduler = m_Scheduler;
                auto* capturedWorld = m_World;
                auto* deltaTimePtr = &m_DeltaTime;
                auto* isExecutingPtr = &m_IsExecuting;

                Task* task = m_Orchestrator->AddTaskToPhase(
                    phaseName,
                    node->metadata.name,
                    [capturedNode, capturedScheduler, capturedWorld, deltaTimePtr, isExecutingPtr]() {
                        // Ensure only one system executes at a time if not parallel
                        if (!capturedNode->metadata.isParallel) {
                            bool expected = false;
                            while (!isExecutingPtr->compare_exchange_weak(expected, true)) {
                                expected = false;
                                std::this_thread::yield();
                            }
                        }

                        try {
                            capturedScheduler->ExecuteSystem(capturedNode, *deltaTimePtr);
                        } catch (...) {
                            if (!capturedNode->metadata.isParallel) {
                                isExecutingPtr->store(false);
                            }
                            throw;
                        }

                        if (!capturedNode->metadata.isParallel) {
                            isExecutingPtr->store(false);
                        }
                    },
                    ConvertPriority(node->metadata.priority)
                );

                nodeToTask[node] = task;

                Logger::Debug(LogECS, "Created task for system '{}' in phase '{}'",
                            node->metadata.name, phaseName);
            }
        }

        // Add dependencies between tasks with validation
        for (const auto& [stage, nodes] : m_Scheduler->GetStageNodes()) {
            for (auto* node : nodes) {
                Task* nodeTask = nodeToTask[node];
                if (!nodeTask) {
                    Logger::Error(LogECS, "Task not found for system '{}'", node->metadata.name);
                    continue;
                }

                for (auto* dep : node->dependencies) {
                    // Only add dependencies within the same stage
                    if (dep->metadata.stage == stage) {
                        Task* depTask = nodeToTask[dep];
                        if (depTask) {
                            nodeTask->AddDependency(depTask);
                            Logger::Debug(LogECS, "Added dependency: '{}' depends on '{}'",
                                        node->metadata.name, dep->metadata.name);
                        } else {
                            Logger::Warn(LogECS, "Dependency task not found for system '{}'",
                                       dep->metadata.name);
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

    ~EngineOrchestratorECS() {
        // Clean up adapter first to ensure no tasks are referencing the scheduler
        m_ECSAdapter.reset();
        // Then clean up the scheduler
        m_SystemScheduler.reset();
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