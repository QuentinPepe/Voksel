module ECS.SystemScheduler;

import Core.Log;
import std;

void SystemScheduler::RegisterSystemMetadata(ISystem* system, const SystemMetadata& metadata) {
    auto node = std::make_unique<SystemNode>();
    node->system = system;
    node->metadata = metadata;
    node->nodeId = static_cast<U32>(m_SystemNodes.size());

    SystemNode* nodePtr = node.get();
    m_SystemNodes[metadata.name] = nodePtr;
    m_StageNodes[metadata.stage].push_back(nodePtr);

    m_NodeStorage.push_back(std::move(node));
}

void SystemScheduler::BuildExecutionGraph(World* world) {
    m_World = world;

    // Resolve explicit dependencies
    ResolveExplicitDependencies();

    // Infer dependencies from component access patterns
    InferComponentDependencies();

    Logger::Info(LogECS, "Built execution graph with {} systems", m_Systems.size());
}

void SystemScheduler::SetExecuteCallback(std::function<void(SystemNode*)> callback) {
    m_ExecuteCallback = std::move(callback);
}

void SystemScheduler::ExecuteSystem(SystemNode* node, F32 dt) {
    auto start = std::chrono::high_resolution_clock::now();

    node->system->Run(m_World, dt);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    m_SystemExecutionTimes[node->metadata.name] = duration;
}

std::string SystemScheduler::GenerateDotGraph() const {
    std::stringstream ss;
    ss << "digraph SystemScheduler {\n";
    ss << "  rankdir=TB;\n";
    ss << "  node [shape=box, style=filled];\n\n";

    // Group by stage
    for (const auto& [stage, nodes] : m_StageNodes) {
        ss << "  subgraph cluster_" << static_cast<int>(stage) << " {\n";
        ss << "    label=\"" << GetStageName(stage) << "\";\n";
        ss << "    style=filled;\n";
        ss << "    color=lightgrey;\n\n";

        for (const auto* node : nodes) {
            std::string color = node->metadata.isParallel ? "lightblue" : "lightcoral";

            // Show read/write info
            std::string label = node->metadata.name;
            if (node->metadata.readComponents != 0 || node->metadata.writeComponents != 0) {
                label += "\\n";
                if (node->metadata.readComponents != 0) {
                    label += "R:" + std::to_string(std::popcount(node->metadata.readComponents));
                }
                if (node->metadata.writeComponents != 0) {
                    if (node->metadata.readComponents != 0) label += " ";
                    label += "W:" + std::to_string(std::popcount(node->metadata.writeComponents));
                }
            }

            ss << "    s" << node->nodeId << " [label=\"" << label
               << "\", fillcolor=" << color << "];\n";
        }
        ss << "  }\n\n";
    }

    // Dependencies
    ss << "  // Dependencies\n";
    for (const auto& [name, node] : m_SystemNodes) {
        for (const auto* dep : node->dependencies) {
            std::string style = "solid";
            if (IsImplicitDependency(node, dep)) {
                style = "dashed";
            }
            ss << "  s" << dep->nodeId << " -> s" << node->nodeId
               << " [style=" << style << "];\n";
        }
    }

    ss << "}\n";
    return ss.str();
}

SystemExecutionStats SystemScheduler::GetStats() const {
    SystemExecutionStats stats{};

    for (const auto& [name, time] : m_SystemExecutionTimes) {
        stats.systemTimes.emplace_back(name, time);
    }

    std::ranges::sort(stats.systemTimes,
                     [](const auto& a, const auto& b) { return a.second > b.second; });

    return stats;
}

const char* SystemScheduler::GetStageName(SystemStage stage) {
    switch (stage) {
        case SystemStage::PreUpdate: return "PreUpdate";
        case SystemStage::Update: return "Update";
        case SystemStage::PostUpdate: return "PostUpdate";
        case SystemStage::PreRender: return "PreRender";
        case SystemStage::Render: return "Render";
        case SystemStage::PostRender: return "PostRender";
        default: return "Unknown";
    }
}

void SystemScheduler::ResolveExplicitDependencies() {
    for (auto& [name, node] : m_SystemNodes) {
        for (const auto& dep : node->metadata.dependencies) {
            auto it = m_SystemNodes.find(dep.targetSystem);
            if (it == m_SystemNodes.end()) {
                Logger::Warn(LogECS, "System '{}' depends on unknown system '{}'",
                            name, dep.targetSystem);
                continue;
            }

            SystemNode* targetNode = it->second;

            switch (dep.type) {
                case SystemDependency::Before:
                    targetNode->dependencies.push_back(node);
                    node->dependents.push_back(targetNode);
                    break;

                case SystemDependency::After:
                    node->dependencies.push_back(targetNode);
                    targetNode->dependents.push_back(node);
                    break;

                case SystemDependency::With:
                    // No explicit dependency for parallel execution
                    break;
            }
        }
    }
}

void SystemScheduler::InferComponentDependencies() {
    // Within each stage, infer dependencies based on component access
    for (auto& [stage, nodes] : m_StageNodes) {
        for (size_t i = 0; i < nodes.size(); ++i) {
            for (size_t j = i + 1; j < nodes.size(); ++j) {
                CheckComponentConflict(nodes[i], nodes[j]);
            }
        }
    }
}

void SystemScheduler::CheckComponentConflict(SystemNode* a, SystemNode* b) {
    // Check for conflicts
    Archetype writeWriteConflict = a->metadata.writeComponents & b->metadata.writeComponents;
    Archetype readWriteConflictA = a->metadata.readComponents & b->metadata.writeComponents;
    Archetype readWriteConflictB = b->metadata.readComponents & a->metadata.writeComponents;

    bool hasConflict = writeWriteConflict != 0 || readWriteConflictA != 0 || readWriteConflictB != 0;

    if (hasConflict && !HasExplicitRelationship(a, b)) {
        // Systems conflict - establish ordering based on priority
        if (a->metadata.priority >= b->metadata.priority) {
            b->dependencies.push_back(a);
            a->dependents.push_back(b);
        } else {
            a->dependencies.push_back(b);
            b->dependents.push_back(a);
        }

        Logger::Debug(LogECS, "Inferred dependency: {} -> {} due to component conflicts",
                     a->metadata.name, b->metadata.name);
    }
}

bool SystemScheduler::HasExplicitRelationship(SystemNode* a, SystemNode* b) const {
    // Check if there's already an explicit dependency between a and b
    if (std::ranges::find(a->dependencies, b) != a->dependencies.end()) return true;
    if (std::ranges::find(b->dependencies, a) != b->dependencies.end()) return true;
    if (std::ranges::find(a->dependents, b) != a->dependents.end()) return true;
    if (std::ranges::find(b->dependents, a) != b->dependents.end()) return true;
    return false;
}

bool SystemScheduler::IsImplicitDependency(const SystemNode* dependent, const SystemNode* dependency) const {
    // Check if this dependency was inferred from component access
    for (const auto& explicitDep : dependent->metadata.dependencies) {
        auto it = m_SystemNodes.find(explicitDep.targetSystem);
        if (it != m_SystemNodes.end() && it->second == dependency) {
            return false;
        }
    }
    return true;
}