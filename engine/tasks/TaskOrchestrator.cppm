export module Tasks.Orchestrator;

import Core.Types;
import Core.Assert;
import Core.Log;
import Tasks.TaskGraph;
import Tasks.TaskProfiler;
import Input.Manager;
import Graphics.Window;
import ECS.World;
import Graphics;
import std;

export class EngineOrchestrator {
public:
    struct FrameData {
        F64 deltaTime;
        F64 totalTime;
        U64 frameNumber;
    };

    struct PhaseNames {
        static constexpr const char *PreFrame = "PreFrame";
        static constexpr const char *Input = "Input";
        static constexpr const char *UserInput = "UserInput";
        static constexpr const char *PreUpdate = "PreUpdate";
        static constexpr const char *Update = "Update";
        static constexpr const char *PostUpdate = "PostUpdate";
        static constexpr const char *PreRender = "PreRender";
        static constexpr const char *Render = "Render";
        static constexpr const char *PostRender = "PostRender";
        static constexpr const char *PostFrame = "PostFrame";
    };

private:
    std::unique_ptr<TaskGraph> m_TaskGraph;

    InputManager *m_InputManager{nullptr};
    Window *m_Window{nullptr};
    World *m_World{nullptr};
    IGraphicsContext *m_Graphics{nullptr};

    FrameData m_CurrentFrame{};
    std::chrono::high_resolution_clock::time_point m_LastFrameTime;
    std::chrono::high_resolution_clock::time_point m_StartTime;

    struct ProfileData {
        std::deque<U64> frameTimes;
        std::deque<U64> phaseTimes[10];
        static constexpr size_t MAX_SAMPLES = 120;

        void AddFrameTime(U64 time) {
            frameTimes.push_back(time);
            if (frameTimes.size() > MAX_SAMPLES) {
                frameTimes.pop_front();
            }
        }

        [[nodiscard]] F64 GetAverageFrameTime() const {
            if (frameTimes.empty()) return 0.0;
            U64 sum = 0;
            for (U64 time: frameTimes) sum += time;
            return static_cast<F64>(sum) / frameTimes.size();
        }
    };

    ProfileData m_ProfileData;
    bool m_ProfilingEnabled{true};
    U32 m_FrameLimitFPS{0};

    std::function<void(FrameData &)> m_PreFrameCallback;
    std::function<void(FrameData &)> m_UpdateCallback;
    std::function<void(FrameData &)> m_RenderCallback;
    std::function<void(FrameData &)> m_PostFrameCallback;
    std::function<void(FrameData &)> m_UserInputCallback;

public:
    explicit EngineOrchestrator(U32 threadCount = 0)
        : m_TaskGraph{std::make_unique<TaskGraph>(threadCount)} {
        m_StartTime = std::chrono::high_resolution_clock::now();
        m_LastFrameTime = m_StartTime;

        SetupPhases();
        Logger::Info(LogTasks, "Engine orchestrator initialized with {} threads", m_TaskGraph->GetThreadCount());
    }

    void SetInputManager(InputManager *inputManager) {
        assert(inputManager, "Input manager cannot be null");
        m_InputManager = inputManager;
    }

    void SetWindow(Window *window) {
        assert(window, "Window cannot be null");
        m_Window = window;
    }

    void SetWorld(World *world) {
        assert(world, "World cannot be null");
        m_World = world;
    }

    void SetGraphicsContext(IGraphicsContext *graphics) {
        assert(graphics, "Graphics context cannot be null");
        m_Graphics = graphics;
    }

    void SetFrameLimit(U32 fps) {
        m_FrameLimitFPS = fps;
    }

    void SetPreFrameCallback(std::function<void(FrameData &)> callback) {
        m_PreFrameCallback = std::move(callback);
    }

    void SetUpdateCallback(std::function<void(FrameData &)> callback) {
        m_UpdateCallback = std::move(callback);
    }

    void SetRenderCallback(std::function<void(FrameData &)> callback) {
        m_RenderCallback = std::move(callback);
    }

    void SetPostFrameCallback(std::function<void(FrameData &)> callback) {
        m_PostFrameCallback = std::move(callback);
    }

    void SetUserInputCallback(std::function<void(FrameData &)> callback) {
        m_UserInputCallback = std::move(callback);
    }

    void ExecuteFrame() {
        auto frameStart = std::chrono::high_resolution_clock::now();

        if (m_ProfilingEnabled) {
            TaskProfiler::Get().BeginFrame(m_CurrentFrame.frameNumber);
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        auto deltaTime = std::chrono::duration<F64>(currentTime - m_LastFrameTime).count();
        auto totalTime = std::chrono::duration<F64>(currentTime - m_StartTime).count();

        m_CurrentFrame.deltaTime = deltaTime;
        m_CurrentFrame.totalTime = totalTime;
        m_CurrentFrame.frameNumber++;
        m_LastFrameTime = currentTime;

        if (m_InputManager) {
            m_InputManager->BeginFrame(m_CurrentFrame.totalTime);
        }

        if (m_Window) {
            m_Window->PollEvents();
        }

        m_TaskGraph->Execute();

        if (m_ProfilingEnabled) {
            TaskProfiler::Get().EndFrame();
            if (m_FrameLimitFPS > 0) {
                F64 targetSec{1.0 / static_cast<F64>(m_FrameLimitFPS)};
                auto now0{std::chrono::high_resolution_clock::now()};
                F64 elapsedSec{std::chrono::duration<F64>(now0 - frameStart).count()};
                if (elapsedSec < targetSec) {
                    auto sleepSec{targetSec - elapsedSec};
                    std::this_thread::sleep_for(std::chrono::duration<F64>{sleepSec});
                }
            }
            auto frameEnd{std::chrono::high_resolution_clock::now()};
            auto frameTime{std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - frameStart).count()};
            m_ProfileData.AddFrameTime(frameTime);
        }
    }

    [[nodiscard]] const FrameData &GetCurrentFrame() const { return m_CurrentFrame; }

    [[nodiscard]] F64 GetAverageFrameTime() const {
        return m_ProfileData.GetAverageFrameTime() / 1000.0;
    }

    [[nodiscard]] F64 GetFPS() const {
        F64 avgFrameTime = GetAverageFrameTime();
        return avgFrameTime > 0.0 ? 1000.0 / avgFrameTime : 0.0;
    }

    [[nodiscard]] std::string GenerateTaskGraphVisualization() const {
        return m_TaskGraph->GenerateDotGraph();
    }

    [[nodiscard]] TaskGraphStats GetTaskGraphStats() const {
        return m_TaskGraph->GetStats();
    }

    void SetProfilingEnabled(bool enabled) {
        m_ProfilingEnabled = enabled;
        m_TaskGraph->SetProfilingEnabled(enabled);
    }

    [[nodiscard]] bool IsProfilingEnabled() const { return m_ProfilingEnabled; }

    Task *AddTaskToPhase(const std::string &phaseName, const std::string &taskName,
                         Task::TaskFunc func, TaskPriority priority = TaskPriority::Normal) {
        TaskPhase *phase = m_TaskGraph->GetPhase(phaseName);
        assert(phase, "Phase not found");
        return phase->AddTask(taskName, std::move(func), priority);
    }

    void AddTaskDependency(const std::string &phaseName, const std::string &taskName,
                           const std::string &dependsOn) {
        TaskPhase *phase = m_TaskGraph->GetPhase(phaseName);
        assert(phase, "Phase not found");
        phase->AddDependency(taskName, dependsOn);
    }

    InputManager *GetInputManager() { return m_InputManager; }
    World *GetWorld() { return m_World; }

private:
    void SetupPhases() {
        auto *preFramePhase = m_TaskGraph->CreatePhase(PhaseNames::PreFrame);
        auto *inputPhase = m_TaskGraph->CreatePhase(PhaseNames::Input);
        auto *userInputPhase = m_TaskGraph->CreatePhase(PhaseNames::UserInput);
        auto *preUpdatePhase = m_TaskGraph->CreatePhase(PhaseNames::PreUpdate);
        auto *updatePhase = m_TaskGraph->CreatePhase(PhaseNames::Update);
        auto *postUpdatePhase = m_TaskGraph->CreatePhase(PhaseNames::PostUpdate);
        auto *preRenderPhase = m_TaskGraph->CreatePhase(PhaseNames::PreRender);
        auto *renderPhase = m_TaskGraph->CreatePhase(PhaseNames::Render);
        auto *postRenderPhase = m_TaskGraph->CreatePhase(PhaseNames::PostRender);
        auto *postFramePhase = m_TaskGraph->CreatePhase(PhaseNames::PostFrame);

        SetupPreFrameTasks(preFramePhase);
        SetupInputTasks(inputPhase);
        SetupUserInputTasks(userInputPhase);
        SetupUpdateTasks(preUpdatePhase, updatePhase, postUpdatePhase);
        SetupRenderTasks(preRenderPhase, renderPhase, postRenderPhase);
        SetupPostFrameTasks(postFramePhase);
    }

    void SetupPreFrameTasks(TaskPhase *phase) {
        phase->AddTask("FrameStart", [this]() {
            if (m_PreFrameCallback) {
                m_PreFrameCallback(m_CurrentFrame);
            }
        }, TaskPriority::Critical);
    }

    void SetupInputTasks(TaskPhase *phase) {
        phase->AddTask("ProcessInputEvents", [this]() {
            if (m_InputManager) {
                m_InputManager->ProcessEvents();
            }
        }, TaskPriority::Critical);
    }

    void SetupUserInputTasks(TaskPhase *phase) {
        phase->AddTask("HandleUserInput", [this]() {
            if (m_UserInputCallback) {
                m_UserInputCallback(m_CurrentFrame);
            }
        }, TaskPriority::High);
    }

    void SetupUpdateTasks(TaskPhase *preUpdate, TaskPhase *update, TaskPhase *postUpdate) {
        preUpdate->AddTask("PreUpdateSystems", [this]() {
            // Pre-update logic
        });

        update->AddTask("UpdateGame", [this]() {
            if (m_UpdateCallback) {
                m_UpdateCallback(m_CurrentFrame);
            }
        });

        postUpdate->AddTask("PostUpdateSystems", [this]() {
            // Post-update logic
        });
    }

    void SetupRenderTasks(TaskPhase *preRender, TaskPhase *render, TaskPhase *postRender) {
        preRender->AddTask("BeginFrame", [this]() {
            if (m_Graphics) {
                m_Graphics->BeginFrame();
            }
        }, TaskPriority::Critical);

        preRender->AddTask("UpdateRenderData", [this]() {
            // Update render data
        });

        render->AddTask("RenderScene", [this]() {
            if (m_RenderCallback) {
                m_RenderCallback(m_CurrentFrame);
            }
        }, TaskPriority::High);

        postRender->AddTask("EndFrame", [this]() {
            if (m_Graphics) {
                m_Graphics->EndFrame();
            }
        }, TaskPriority::Critical);

        postRender->AddTask("RenderStats", [this]() {
            // Render statistics
        });
    }

    void SetupPostFrameTasks(TaskPhase *phase) {
        phase->AddTask("FrameEnd", [this]() {
            if (m_PostFrameCallback) {
                m_PostFrameCallback(m_CurrentFrame);
            }
        });
    }
};
