export module Tasks.TaskGraph;

import Core.Types;
import Core.Assert;
import Core.Log;
import Tasks.TaskProfiler;
import std;

export enum class TaskStatus : U8 {
    Pending,
    Running,
    Completed,
    Failed
};

export enum class TaskPriority : S8 {
    Critical = 127,
    High = 64,
    Normal = 0,
    Low = -64,
    Idle = -128
};

export class TaskGraph;
export class TaskPhase;

export class Task {
    friend class TaskGraph;
    friend class TaskExecutor;
    friend class TaskPhase;

public:
    using TaskFunc = std::function<void()>;

private:
    std::string m_Name;
    TaskFunc m_Function;
    Vector<Task*> m_Dependencies;
    Vector<Task*> m_Dependents;
    std::atomic<TaskStatus> m_Status{TaskStatus::Pending};
    std::atomic<U32> m_PendingDependencies{0};
    TaskPriority m_Priority{TaskPriority::Normal};
    U64 m_ExecutionTime{0};
    U32 m_TaskID;
    U32 m_PhaseID{0};

    std::chrono::high_resolution_clock::time_point m_StartTime;
    std::chrono::high_resolution_clock::time_point m_EndTime;

    void SetPhaseID(U32 phaseID) { m_PhaseID = phaseID; }

public:
    Task(std::string name, TaskFunc func, TaskPriority priority = TaskPriority::Normal)
        : m_Name{std::move(name)}, m_Function{std::move(func)}, m_Priority{priority} {
        static std::atomic<U32> s_NextID{0};
        m_TaskID = s_NextID.fetch_add(1);
    }

    void Execute() {
        m_StartTime = std::chrono::high_resolution_clock::now();
        m_Status.store(TaskStatus::Running);

        try {
            if (m_Function) {
                m_Function();
            }
            m_Status.store(TaskStatus::Completed);
        } catch (const std::exception& e) {
            Logger::Error(LogTasks, "Task '{}' failed: {}", m_Name, e.what());
            m_Status.store(TaskStatus::Failed);
        }

        m_EndTime = std::chrono::high_resolution_clock::now();
        m_ExecutionTime = std::chrono::duration_cast<std::chrono::microseconds>(
            m_EndTime - m_StartTime).count();
    }

    void AddDependency(Task* dependency) {
        assert(dependency != this, "Task cannot depend on itself");
        m_Dependencies.push_back(dependency);
        dependency->m_Dependents.push_back(this);
    }

    void Reset() {
        m_Status.store(TaskStatus::Pending);
        m_PendingDependencies.store(static_cast<U32>(m_Dependencies.size()));
        m_ExecutionTime = 0;
    }

    [[nodiscard]] TaskStatus GetStatus() const { return m_Status.load(); }
    [[nodiscard]] const std::string& GetName() const { return m_Name; }
    [[nodiscard]] U32 GetID() const { return m_TaskID; }
    [[nodiscard]] U64 GetExecutionTime() const { return m_ExecutionTime; }
    [[nodiscard]] TaskPriority GetPriority() const { return m_Priority; }
    [[nodiscard]] U32 GetPhaseID() const { return m_PhaseID; }
    [[nodiscard]] U64 GetStartTimestamp() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            m_StartTime.time_since_epoch()).count();
    }
    [[nodiscard]] U64 GetEndTimestamp() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            m_EndTime.time_since_epoch()).count();
    }
};

class TaskPhase {
    friend class TaskGraph;

private:
    std::string m_Name;
    Vector<std::unique_ptr<Task>> m_Tasks;
    UnorderedMap<std::string, Task*> m_TaskMap;
    U32 m_PhaseID;
    std::atomic<bool> m_Completed{false};

public:
    explicit TaskPhase(std::string name, U32 id)
        : m_Name{std::move(name)}, m_PhaseID{id} {}

    Task* AddTask(const std::string& name, Task::TaskFunc func, TaskPriority priority = TaskPriority::Normal) {
        auto task = std::make_unique<Task>(name, std::move(func), priority);
        task->SetPhaseID(m_PhaseID);
        Task* ptr = task.get();
        m_TaskMap[name] = ptr;
        m_Tasks.push_back(std::move(task));
        return ptr;
    }

    Task* GetTask(const std::string& name) {
        auto it = m_TaskMap.find(name);
        return it != m_TaskMap.end() ? it->second : nullptr;
    }

    void AddDependency(const std::string& taskName, const std::string& dependsOn) {
        Task* task = GetTask(taskName);
        Task* dependency = GetTask(dependsOn);

        assert(task, "Task not found");
        assert(dependency, "Dependency task not found");
        assert(task != dependency, "Task cannot depend on itself");

        task->AddDependency(dependency);
    }

    void AddDependency(Task* task, Task* dependency) {
        assert(task, "Task cannot be null");
        assert(dependency, "Dependency cannot be null");
        assert(task->GetPhaseID() == m_PhaseID, "Task must belong to this phase");
        assert(dependency->GetPhaseID() == m_PhaseID, "Dependency must belong to this phase");

        task->AddDependency(dependency);
    }

    void Reset() {
        m_Completed.store(false);
        for (auto& task : m_Tasks) {
            task->Reset();
        }
    }

    [[nodiscard]] const std::string& GetName() const { return m_Name; }
    [[nodiscard]] U32 GetID() const { return m_PhaseID; }
    [[nodiscard]] bool IsCompleted() const { return m_Completed.load(); }
    [[nodiscard]] const Vector<std::unique_ptr<Task>>& GetTasks() const { return m_Tasks; }
};

class TaskExecutor {
private:
    struct WorkerThread {
        std::thread thread;
        std::atomic<bool> shouldStop{false};
        U32 threadID;
        U64 totalExecutionTime{0};
        U32 tasksExecuted{0};
    };

    Vector<std::unique_ptr<WorkerThread>> m_Workers;
    std::queue<Task*> m_ReadyQueue;
    std::mutex m_QueueMutex;
    std::condition_variable m_QueueCV;
    std::atomic<U32> m_ActiveTasks{0};
    std::atomic<bool> m_Running{false};
    bool m_ProfilingEnabled{true};

    std::chrono::steady_clock::time_point m_LastProgressTime;
    std::atomic<U32> m_LastActiveCount{0};

public:
    explicit TaskExecutor(U32 threadCount = 0) {
        if (threadCount == 0) {
            threadCount = std::thread::hardware_concurrency();
            if (threadCount == 0) threadCount = 4;
        }

        m_Workers.reserve(threadCount);
        for (U32 i = 0; i < threadCount; ++i) {
            auto worker = std::make_unique<WorkerThread>();
            worker->threadID = i;
            worker->thread = std::thread(&TaskExecutor::WorkerLoop, this, worker.get());
            m_Workers.push_back(std::move(worker));
        }

        m_Running.store(true);
        m_LastProgressTime = std::chrono::steady_clock::now();
        Logger::Info(LogTasks, "TaskExecutor initialized with {} worker threads", threadCount);
    }

    ~TaskExecutor() {
        Shutdown();
    }

    void SubmitTask(Task* task) {
        if (!task) return;

        {
            std::lock_guard lock(m_QueueMutex);
            m_ReadyQueue.push(task);
        }
        m_QueueCV.notify_one();
    }

    void WaitForCompletion() {
        constexpr auto timeout = std::chrono::seconds(30);
        constexpr auto checkInterval = std::chrono::milliseconds(100);
        auto start = std::chrono::steady_clock::now();

        std::unique_lock lock(m_QueueMutex);

        while (!m_ReadyQueue.empty() || m_ActiveTasks.load() > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = now - start;

            if (elapsed > timeout) {
                Logger::Error(LogTasks, "WaitForCompletion timeout! Queue size: {}, Active tasks: {}",
                             m_ReadyQueue.size(), m_ActiveTasks.load());

                break;
            }

            bool signaled = m_QueueCV.wait_for(lock, checkInterval, [this] {
                return m_ReadyQueue.empty() && m_ActiveTasks.load() == 0;
            });

            U32 currentActive = m_ActiveTasks.load();
            if (currentActive != m_LastActiveCount.load()) {
                m_LastActiveCount.store(currentActive);
                m_LastProgressTime = now;
            } else if (currentActive > 0) {

                auto timeSinceProgress = now - m_LastProgressTime;
                if (timeSinceProgress > std::chrono::seconds(5)) {
                    Logger::Warn(LogTasks, "No progress for {}s. Active tasks: {}, Queue size: {}",
                                std::chrono::duration_cast<std::chrono::seconds>(timeSinceProgress).count(),
                                currentActive, m_ReadyQueue.size());
                }
            }

            if (m_ReadyQueue.empty() && m_ActiveTasks.load() == 0) {
                break;
            }
        }
    }

    void Shutdown() {
        if (!m_Running.load()) return;

        m_Running.store(false);
        m_QueueCV.notify_all();

        for (auto& worker : m_Workers) {
            worker->shouldStop.store(true);
            if (worker->thread.joinable()) {
                worker->thread.join();
            }
        }

        Logger::Info("TaskExecutor shutdown complete");
    }

    void SetProfilingEnabled(bool enabled) { m_ProfilingEnabled = enabled; }
    [[nodiscard]] U32 GetThreadCount() const { return static_cast<U32>(m_Workers.size()); }

private:
    void WorkerLoop(WorkerThread* worker) {
        while (m_Running.load()) {
            Task* task = nullptr;

            {
                std::unique_lock lock(m_QueueMutex);

                if (m_ReadyQueue.empty() && m_ActiveTasks.load() == 0) {
                    m_QueueCV.notify_all();
                }

                m_QueueCV.wait(lock, [this, worker] {
                    return !m_ReadyQueue.empty() || !m_Running.load() || worker->shouldStop.load();
                });

                if (!m_Running.load() || worker->shouldStop.load()) break;

                if (!m_ReadyQueue.empty()) {
                    task = m_ReadyQueue.front();
                    m_ReadyQueue.pop();
                    m_ActiveTasks.fetch_add(1);
                }
            }

            if (task) {
                task->Execute();
                worker->tasksExecuted++;
                worker->totalExecutionTime += task->GetExecutionTime();

                if (m_ProfilingEnabled && TaskProfiler::Get().IsEnabled()) {
                    TaskProfiler::Get().RecordTask(
                        task->GetName(),
                        task->GetID(),
                        task->GetPhaseID(),
                        task->GetStartTimestamp(),
                        task->GetEndTimestamp()
                    );
                }

                {
                    std::lock_guard lock(m_QueueMutex);

                    for (Task* dependent : task->m_Dependents) {
                        if (dependent->GetStatus() == TaskStatus::Pending) {
                            U32 remaining = dependent->m_PendingDependencies.fetch_sub(1);
                            if (remaining == 1) {
                                m_ReadyQueue.push(dependent);
                                m_QueueCV.notify_one();
                            }
                        }
                    }
                }

                U32 currentActive = m_ActiveTasks.fetch_sub(1) - 1;

                {
                    std::lock_guard lock(m_QueueMutex);
                    if (currentActive == 0 && m_ReadyQueue.empty()) {
                        m_QueueCV.notify_all();
                    }
                }
            }
        }
    }
};

export struct TaskGraphStats {
    U64 totalExecutionTime;
    U32 totalTasks;
    U32 completedTasks;
    U32 failedTasks;
    U32 activePhases;
    Vector<std::pair<std::string, U64>> taskTimings;
};

class TaskGraph {
private:
    Vector<std::unique_ptr<TaskPhase>> m_Phases;
    UnorderedMap<std::string, TaskPhase*> m_PhaseMap;
    std::unique_ptr<TaskExecutor> m_Executor;
    std::atomic<bool> m_Running{false};
    U32 m_NextPhaseID{0};
    bool m_ProfilingEnabled{true};

    std::chrono::high_resolution_clock::time_point m_GraphStartTime;
    std::chrono::high_resolution_clock::time_point m_GraphEndTime;
    TaskGraphStats m_LastStats;

public:
    explicit TaskGraph(U32 threadCount = 0)
        : m_Executor{std::make_unique<TaskExecutor>(threadCount)} {}

    TaskPhase* CreatePhase(const std::string& name) {
        auto phase = std::make_unique<TaskPhase>(name, m_NextPhaseID++);
        TaskPhase* ptr = phase.get();
        m_Phases.push_back(std::move(phase));
        m_PhaseMap[name] = ptr;
        return ptr;
    }

    TaskPhase* GetPhase(const std::string& name) {
        auto it = m_PhaseMap.find(name);
        return it != m_PhaseMap.end() ? it->second : nullptr;
    }

    void Execute() {
        assert(!m_Running.load(), "TaskGraph is already running");
        m_Running.store(true);

        m_GraphStartTime = std::chrono::high_resolution_clock::now();

        for (auto& phase : m_Phases) {
            phase->Reset();
        }

        for (auto& phase : m_Phases) {
            ExecutePhase(phase.get());
        }

        m_GraphEndTime = std::chrono::high_resolution_clock::now();
        m_Running.store(false);

        UpdateStats();
    }

    void ExecutePhase(TaskPhase* phase) const {
        if (m_ProfilingEnabled && TaskProfiler::Get().IsEnabled()) {
            TaskProfiler::Get().BeginPhase(phase->GetName(), phase->GetID());
        }

        U32 submittedCount = 0;
        for (const auto& task : phase->GetTasks()) {
            if (task->m_Dependencies.empty()) {
                m_Executor->SubmitTask(task.get());
                submittedCount++;
            }
        }

        if (submittedCount == 0 && !phase->GetTasks().empty()) {
            Logger::Error(LogTasks, "No tasks in phase '{}' could be submitted - possible circular dependency",
                         phase->GetName());

            for (const auto& task : phase->GetTasks()) {
                Logger::Error(LogTasks, "  Task '{}' has {} dependencies:",
                             task->GetName(), task->m_Dependencies.size());
                for (const Task* dep : task->m_Dependencies) {
                    Logger::Error(LogTasks, "    -> '{}'", dep->GetName());
                }
            }
        }

        m_Executor->WaitForCompletion();

        bool allCompleted = true;
        U32 pendingCount = 0;
        U32 runningCount = 0;
        U32 failedCount = 0;

        for (const auto& task : phase->GetTasks()) {
            TaskStatus status = task->GetStatus();
            if (status != TaskStatus::Completed) {
                allCompleted = false;

                if (status == TaskStatus::Failed) {
                    failedCount++;
                    Logger::Error(LogTasks, "Task '{}' in phase '{}' failed",
                                 task->GetName(), phase->GetName());
                } else if (status == TaskStatus::Pending) {
                    pendingCount++;
                    Logger::Error(LogTasks, "Task '{}' in phase '{}' was never executed (status: Pending)",
                                 task->GetName(), phase->GetName());

                    Logger::Debug(LogTasks, "  Task has {} dependencies, {} pending",
                                 task->m_Dependencies.size(),
                                 task->m_PendingDependencies.load());

                    for (const Task* dep : task->m_Dependencies) {
                        Logger::Debug(LogTasks, "  Dependency '{}' status: {}",
                                    dep->GetName(),
                                    static_cast<int>(dep->GetStatus()));
                    }
                } else if (status == TaskStatus::Running) {
                    runningCount++;
                    Logger::Error(LogTasks, "Task '{}' in phase '{}' is still running - possible deadlock",
                                 task->GetName(), phase->GetName());
                }
            }
        }

        if (!allCompleted) {
            Logger::Error(LogTasks, "Phase '{}' incomplete: {} pending, {} running, {} failed",
                         phase->GetName(), pendingCount, runningCount, failedCount);
        }

        phase->m_Completed.store(allCompleted);

        if (m_ProfilingEnabled && TaskProfiler::Get().IsEnabled()) {
            TaskProfiler::Get().EndPhase(phase->GetID());
        }
    }

    [[nodiscard]] std::string GenerateDotGraph() const {
        std::stringstream ss;
        ss << "digraph TaskGraph {\n";
        ss << "  rankdir=TB;\n";
        ss << "  node [shape=box, style=filled];\n\n";

        for (const auto& phase : m_Phases) {
            ss << "  subgraph cluster_" << phase->GetID() << " {\n";
            ss << "    label=\"" << phase->GetName() << "\";\n";
            ss << "    style=filled;\n";
            ss << "    color=lightgrey;\n\n";

            for (const auto& task : phase->GetTasks()) {
                std::string color = "lightblue";
                if (task->GetStatus() == TaskStatus::Completed) color = "lightgreen";
                else if (task->GetStatus() == TaskStatus::Failed) color = "lightcoral";
                else if (task->GetStatus() == TaskStatus::Running) color = "yellow";

                ss << "    t" << task->GetID() << " [label=\"" << task->GetName()
                   << "\\n" << task->GetExecutionTime() << "μs\", fillcolor=" << color << "];\n";
            }
            ss << "  }\n\n";
        }

        ss << "  // Dependencies\n";
        for (const auto& phase : m_Phases) {
            for (const auto& task : phase->GetTasks()) {
                for (const Task* dep : task->m_Dependencies) {
                    ss << "  t" << dep->GetID() << " -> t" << task->GetID() << ";\n";
                }
            }
        }

        ss << "\n  // Phase ordering\n";
        ss << "  edge [style=dashed, color=red];\n";
        for (size_t i = 1; i < m_Phases.size(); ++i) {
            const auto& prevPhase = m_Phases[i - 1];
            const auto& currPhase = m_Phases[i];

            if (!prevPhase->GetTasks().empty() && !currPhase->GetTasks().empty()) {
                ss << "  t" << prevPhase->GetTasks().back()->GetID()
                   << " -> t" << currPhase->GetTasks().front()->GetID()
                   << " [constraint=false];\n";
            }
        }

        ss << "}\n";
        return ss.str();
    }

    [[nodiscard]] const TaskGraphStats& GetStats() const { return m_LastStats; }
    [[nodiscard]] U32 GetThreadCount() const { return m_Executor->GetThreadCount(); }

    void SetProfilingEnabled(bool enabled) {
        m_ProfilingEnabled = enabled;
        m_Executor->SetProfilingEnabled(enabled);
    }

private:
    void UpdateStats() {
        m_LastStats = {};

        auto graphTime = std::chrono::duration_cast<std::chrono::microseconds>(
            m_GraphEndTime - m_GraphStartTime).count();
        m_LastStats.totalExecutionTime = graphTime;

        for (const auto& phase : m_Phases) {
            if (phase->IsCompleted()) m_LastStats.activePhases++;

            for (const auto& task : phase->GetTasks()) {
                m_LastStats.totalTasks++;

                if (task->GetStatus() == TaskStatus::Completed) {
                    m_LastStats.completedTasks++;
                    m_LastStats.taskTimings.emplace_back(task->GetName(), task->GetExecutionTime());
                } else if (task->GetStatus() == TaskStatus::Failed) {
                    m_LastStats.failedTasks++;
                }
            }
        }

        std::ranges::sort(m_LastStats.taskTimings,
                         [](const auto& a, const auto& b) { return a.second > b.second; });
    }
};