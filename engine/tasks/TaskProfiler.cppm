export module Tasks.TaskProfiler;

import Core.Types;
import Core.Log;
import std;

export struct TaskProfile {
    std::string name;
    U64 startTime;
    U64 endTime;
    U64 duration;
    U32 threadID;
    U32 taskID;
    U32 phaseID;

    [[nodiscard]] U64 GetDurationMicros() const { return duration; }
    [[nodiscard]] F64 GetDurationMillis() const { return duration / 1000.0; }
};

export struct PhaseProfile {
    std::string name;
    U32 phaseID;
    U64 startTime;
    U64 endTime;
    U64 duration;
    Vector<TaskProfile> tasks;

    [[nodiscard]] U64 GetTotalTaskTime() const {
        U64 total = 0;
        for (const auto& task : tasks) {
            total += task.duration;
        }
        return total;
    }

    [[nodiscard]] F64 GetParallelEfficiency() const {
        if (duration == 0) return 0.0;
        return static_cast<F64>(GetTotalTaskTime()) / duration;
    }
};

export struct FrameProfile {
    U64 frameNumber;
    U64 startTime;
    U64 endTime;
    U64 duration;
    Vector<PhaseProfile> phases;

    [[nodiscard]] const PhaseProfile* GetPhase(const std::string& name) const {
        auto it = std::ranges::find_if(phases, [&name](const auto& p) { return p.name == name; });
        return it != phases.end() ? &(*it) : nullptr;
    }
};

export class TaskProfiler {
private:
    struct ThreadLocalData {
        Vector<TaskProfile> currentFrameTasks;
        U32 threadID;
    };

    static constexpr size_t MAX_FRAME_HISTORY = 300;

    std::deque<FrameProfile> m_FrameHistory;
    FrameProfile m_CurrentFrame;

    std::mutex m_Mutex;
    UnorderedMap<std::thread::id, ThreadLocalData> m_ThreadData;
    std::atomic<U32> m_NextThreadID{0};

    bool m_Enabled{true};
    bool m_DetailedProfiling{false};

    // Statistics
    struct Stats {
        F64 avgFrameTime{0.0};
        F64 minFrameTime{std::numeric_limits<F64>::max()};
        F64 maxFrameTime{0.0};
        F64 frameTimeStdDev{0.0};
        UnorderedMap<std::string, F64> avgPhaseTimes;
        UnorderedMap<std::string, F64> avgTaskTimes;
    };

    Stats m_CachedStats;
    bool m_StatsDirty{true};

public:
    static TaskProfiler& Get() {
        static TaskProfiler instance;
        return instance;
    }

    void BeginFrame(U64 frameNumber) {
        if (!m_Enabled) return;

        std::lock_guard lock(m_Mutex);

        // Store previous frame
        if (m_CurrentFrame.frameNumber > 0) {
            m_FrameHistory.push_back(std::move(m_CurrentFrame));
            if (m_FrameHistory.size() > MAX_FRAME_HISTORY) {
                m_FrameHistory.pop_front();
            }
            m_StatsDirty = true;
        }

        // Start new frame
        m_CurrentFrame = {};
        m_CurrentFrame.frameNumber = frameNumber;
        m_CurrentFrame.startTime = GetTimestamp();

        // Clear thread data
        for (auto& [id, data] : m_ThreadData) {
            data.currentFrameTasks.clear();
        }
    }

    void EndFrame() {
        if (!m_Enabled) return;

        std::lock_guard lock(m_Mutex);
        m_CurrentFrame.endTime = GetTimestamp();
        m_CurrentFrame.duration = m_CurrentFrame.endTime - m_CurrentFrame.startTime;
    }

    void BeginPhase(const std::string& name, U32 phaseID) {
        if (!m_Enabled) return;

        std::lock_guard lock(m_Mutex);
        PhaseProfile phase;
        phase.name = name;
        phase.phaseID = phaseID;
        phase.startTime = GetTimestamp();
        m_CurrentFrame.phases.push_back(std::move(phase));
    }

    void EndPhase(U32 phaseID) {
        if (!m_Enabled) return;

        std::lock_guard lock(m_Mutex);
        auto it = std::ranges::find_if(m_CurrentFrame.phases,
                                      [phaseID](const auto& p) { return p.phaseID == phaseID; });

        if (it != m_CurrentFrame.phases.end()) {
            it->endTime = GetTimestamp();
            it->duration = it->endTime - it->startTime;

            // Collect tasks from all threads for this phase
            for (auto& [id, data] : m_ThreadData) {
                for (const auto& task : data.currentFrameTasks) {
                    if (task.phaseID == phaseID) {
                        it->tasks.push_back(task);
                    }
                }
            }
        }
    }

    void RecordTask(const std::string& name, U32 taskID, U32 phaseID,
                    U64 startTime, U64 endTime) {
        if (!m_Enabled) return;

        auto threadID = std::this_thread::get_id();

        std::lock_guard lock(m_Mutex);

        // Get or create thread data
        if (!m_ThreadData.contains(threadID)) {
            m_ThreadData[threadID].threadID = m_NextThreadID.fetch_add(1);
        }

        TaskProfile task;
        task.name = name;
        task.taskID = taskID;
        task.phaseID = phaseID;
        task.startTime = startTime;
        task.endTime = endTime;
        task.duration = endTime - startTime;
        task.threadID = m_ThreadData[threadID].threadID;

        m_ThreadData[threadID].currentFrameTasks.push_back(std::move(task));
    }

    [[nodiscard]] std::string GenerateReport() const {
        const_cast<TaskProfiler*>(this)->UpdateStats();

        std::stringstream ss;
        ss << "=== Task Profiler Report ===\n\n";

        ss << "Frame Statistics:\n";
        ss << std::format("  Average: {:.2f}ms\n", m_CachedStats.avgFrameTime);
        ss << std::format("  Min: {:.2f}ms\n", m_CachedStats.minFrameTime);
        ss << std::format("  Max: {:.2f}ms\n", m_CachedStats.maxFrameTime);
        ss << std::format("  StdDev: {:.2f}ms\n", m_CachedStats.frameTimeStdDev);
        ss << std::format("  FPS: {:.1f}\n", 1000.0 / m_CachedStats.avgFrameTime);

        ss << "\nPhase Timings:\n";
        Vector<std::pair<std::string, F64>> sortedPhases(m_CachedStats.avgPhaseTimes.begin(),
                                                         m_CachedStats.avgPhaseTimes.end());
        std::ranges::sort(sortedPhases, [](const auto& a, const auto& b) { return a.second > b.second; });

        for (const auto& [name, time] : sortedPhases) {
            ss << std::format("  {}: {:.2f}ms\n", name, time);
        }

        if (m_DetailedProfiling) {
            ss << "\nTop 10 Tasks:\n";
            Vector<std::pair<std::string, F64>> sortedTasks(m_CachedStats.avgTaskTimes.begin(),
                                                           m_CachedStats.avgTaskTimes.end());
            std::ranges::sort(sortedTasks, [](const auto& a, const auto& b) { return a.second > b.second; });

            for (size_t i = 0; i < std::min<size_t>(10, sortedTasks.size()); ++i) {
                ss << std::format("  {}: {:.2f}ms\n", sortedTasks[i].first, sortedTasks[i].second);
            }
        }

        return ss.str();
    }

    [[nodiscard]] std::string GenerateCSV() const {
        std::stringstream ss;
        ss << "Frame,Phase,Task,Thread,Start,End,Duration\n";

        for (const auto& frame : m_FrameHistory) {
            for (const auto& phase : frame.phases) {
                for (const auto& task : phase.tasks) {
                    ss << frame.frameNumber << ","
                       << phase.name << ","
                       << task.name << ","
                       << task.threadID << ","
                       << task.startTime << ","
                       << task.endTime << ","
                       << task.duration << "\n";
                }
            }
        }

        return ss.str();
    }

    [[nodiscard]] std::string GenerateFlameGraph() const {
        // Generate data in format suitable for flamegraph tools
        std::stringstream ss;

        if (!m_FrameHistory.empty()) {
            const auto& lastFrame = m_FrameHistory.back();

            for (const auto& phase : lastFrame.phases) {
                for (const auto& task : phase.tasks) {
                    // Format: stack;frame duration
                    ss << "Frame;" << phase.name << ";" << task.name
                       << " " << task.duration << "\n";
                }
            }
        }

        return ss.str();
    }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return m_Enabled; }

    void SetDetailedProfiling(bool detailed) { m_DetailedProfiling = detailed; }
    [[nodiscard]] bool IsDetailedProfiling() const { return m_DetailedProfiling; }

    void SaveToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << GenerateReport();
            file << "\n\n";
            file << GenerateCSV();
            Logger::Info(LogTasks, "Profiler data saved to {}", filename);
        } else {
            Logger::Error(LogTasks, "Failed to save profiler data to {}", filename);
        }
    }

    [[nodiscard]] const Stats& GetStats() const {
        const_cast<TaskProfiler*>(this)->UpdateStats();
        return m_CachedStats;
    }

    void Clear() {
        std::lock_guard lock(m_Mutex);
        m_FrameHistory.clear();
        m_CurrentFrame = {};
        m_ThreadData.clear();
        m_CachedStats = {};
        m_StatsDirty = true;
    }

private:
    TaskProfiler() = default;

    [[nodiscard]] static U64 GetTimestamp() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }

    void UpdateStats() {
        if (!m_StatsDirty) return;

        std::lock_guard lock(m_Mutex);

        m_CachedStats = {};

        if (m_FrameHistory.empty()) return;

        // Frame statistics
        Vector<F64> frameTimes;
        for (const auto& frame : m_FrameHistory) {
            F64 frameTimeMs = frame.duration / 1000.0;
            frameTimes.push_back(frameTimeMs);
            m_CachedStats.minFrameTime = std::min(m_CachedStats.minFrameTime, frameTimeMs);
            m_CachedStats.maxFrameTime = std::max(m_CachedStats.maxFrameTime, frameTimeMs);
        }

        // Average frame time
        F64 sum = 0.0;
        for (F64 time : frameTimes) sum += time;
        m_CachedStats.avgFrameTime = sum / frameTimes.size();

        // Standard deviation
        F64 variance = 0.0;
        for (F64 time : frameTimes) {
            F64 diff = time - m_CachedStats.avgFrameTime;
            variance += diff * diff;
        }
        m_CachedStats.frameTimeStdDev = std::sqrt(variance / frameTimes.size());

        // Phase and task statistics
        UnorderedMap<std::string, Vector<F64>> phaseTimes;
        UnorderedMap<std::string, Vector<F64>> taskTimes;

        for (const auto& frame : m_FrameHistory) {
            for (const auto& phase : frame.phases) {
                phaseTimes[phase.name].push_back(phase.duration / 1000.0);

                for (const auto& task : phase.tasks) {
                    taskTimes[task.name].push_back(task.duration / 1000.0);
                }
            }
        }

        // Calculate averages
        for (const auto& [name, times] : phaseTimes) {
            F64 phaseSum = 0.0;
            for (F64 time : times) phaseSum += time;
            m_CachedStats.avgPhaseTimes[name] = phaseSum / times.size();
        }

        for (const auto& [name, times] : taskTimes) {
            F64 taskSum = 0.0;
            for (F64 time : times) taskSum += time;
            m_CachedStats.avgTaskTimes[name] = taskSum / times.size();
        }

        m_StatsDirty = false;
    }
};

// RAII helper for profiling
export class ScopedProfiler {
private:
    std::string m_Name;
    U32 m_TaskID;
    U32 m_PhaseID;
    U64 m_StartTime;

public:
    ScopedProfiler(std::string name, U32 taskID, U32 phaseID)
        : m_Name{std::move(name)}, m_TaskID{taskID}, m_PhaseID{phaseID} {
        m_StartTime = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }

    ~ScopedProfiler() {
        U64 endTime = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        TaskProfiler::Get().RecordTask(m_Name, m_TaskID, m_PhaseID, m_StartTime, endTime);
    }
};