module;
#include <d3d12.h>

export module Graphics.DX12.RenderGraph;

import Core.Types;
import Core.Assert;
import Core.Log;
import Graphics.DX12.Device;
import Graphics.DX12.Resource;
import Graphics.DX12.CommandList;
import Graphics.DX12.DescriptorHeap;
import std;

export enum class ResourceUsageType : U8 {
    Read,
    Write,
    ReadWrite
};

export struct RenderGraphResourceHandle {
    U32 index = INVALID_INDEX;
    U32 version = 0;

    bool IsValid() const { return index != INVALID_INDEX; }
    bool operator==(const RenderGraphResourceHandle& other) const {
        return index == other.index && version == other.version;
    }
};

struct ResourceNode {
    std::string name;
    enum Type { Buffer, Texture } type;
    union {
        BufferDesc bufferDesc;
        TextureDesc textureDesc;
    };

    std::unique_ptr<Resource> resource;
    Vector<U32> writerPasses;
    Vector<U32> readerPasses;
    U32 firstPass = INVALID_INDEX;
    U32 lastPass = INVALID_INDEX;
    bool isImported = false;
    bool isCulled = false;

    ResourceNode(const std::string& n, const BufferDesc& desc)
        : name{n}, type{Buffer}, bufferDesc{desc} {}

    ResourceNode(const std::string& n, const TextureDesc& desc)
        : name{n}, type{Texture}, textureDesc{desc} {}
};

struct PassNode {
    std::string name;
    std::function<void(class RenderGraphBuilder&)> setup;
    std::function<void(const class RenderGraphResources&, CommandList&)> execute;

    Vector<RenderGraphResourceHandle> reads;
    Vector<RenderGraphResourceHandle> writes;
    Vector<U32> dependencies;

    bool isCulled = false;
    U32 executionOrder = INVALID_INDEX;
};

export class RenderGraphBuilder {
private:
    class RenderGraph* m_Graph;
    U32 m_PassIndex;

public:
    RenderGraphBuilder(RenderGraph* graph, U32 passIndex)
        : m_Graph{graph}, m_PassIndex{passIndex} {}

    RenderGraphResourceHandle CreateBuffer(const std::string& name, const BufferDesc& desc);
    RenderGraphResourceHandle CreateTexture(const std::string& name, const TextureDesc& desc);
    RenderGraphResourceHandle ImportBuffer(const std::string& name, Buffer* buffer);
    RenderGraphResourceHandle ImportTexture(const std::string& name, Texture* texture);

    void ReadBuffer(RenderGraphResourceHandle handle);
    void WriteBuffer(RenderGraphResourceHandle handle);
    void ReadTexture(RenderGraphResourceHandle handle);
    void WriteTexture(RenderGraphResourceHandle handle);
};

export class RenderGraphResources {
private:
    const Vector<ResourceNode>* m_Resources;
    CommandList* m_CommandList;

public:
    RenderGraphResources(const Vector<ResourceNode>* resources, CommandList* cmdList)
        : m_Resources{resources}, m_CommandList{cmdList} {}

    Buffer* GetBuffer(RenderGraphResourceHandle handle) const {
        assert(handle.IsValid() && handle.index < m_Resources->size(), "Invalid resource handle");
        auto& node = (*m_Resources)[handle.index];
        assert(node.type == ResourceNode::Buffer, "Resource is not a buffer");
        return static_cast<Buffer*>(node.resource.get());
    }

    Texture* GetTexture(RenderGraphResourceHandle handle) const {
        assert(handle.IsValid() && handle.index < m_Resources->size(), "Invalid resource handle");
        auto& node = (*m_Resources)[handle.index];
        assert(node.type == ResourceNode::Texture, "Resource is not a texture");
        return static_cast<Texture*>(node.resource.get());
    }

    CommandList& GetCommandList() { return *m_CommandList; }
};

export class RenderGraph {
private:
    Device* m_Device;
    Vector<ResourceNode> m_Resources;
    Vector<PassNode> m_Passes;
    Vector<U32> m_ExecutionOrder;
    U32 m_ResourceVersionCounter = 0;

    friend class RenderGraphBuilder;

public:
    explicit RenderGraph(Device& device) : m_Device{&device} {}

    template<typename PassData>
    void AddPass(const std::string& name,
                 std::function<void(RenderGraphBuilder&, PassData&)> setup,
                 std::function<void(const RenderGraphResources&, CommandList&, const PassData&)> execute) {

        auto passData = std::make_shared<PassData>();

        PassNode& pass = m_Passes.emplace_back();
        pass.name = name;

        pass.setup = [this, setup, passData, passIndex = m_Passes.size() - 1](RenderGraphBuilder& builder) {
            setup(builder, *passData);
        };

        pass.execute = [execute, passData](const RenderGraphResources& resources, CommandList& cmdList) {
            execute(resources, cmdList, *passData);
        };
    }

    void Clear() {
        m_Resources.clear();
        m_Passes.clear();
        m_ExecutionOrder.clear();
        m_ResourceVersionCounter = 0;
    }

    void Compile() {
        // Phase 1: Execute setup functions
        for (U32 i = 0; i < m_Passes.size(); ++i) {
            RenderGraphBuilder builder(this, i);
            m_Passes[i].setup(builder);
        }

        // Phase 2: Build dependency graph
        BuildDependencies();

        // Phase 3: Cull unused passes and resources
        CullUnusedPasses();

        // Phase 4: Calculate execution order
        CalculateExecutionOrder();

        // Phase 5: Calculate resource lifetimes
        CalculateResourceLifetimes();

        Logger::Trace("Render graph compiled: {} passes, {} resources",
                     std::ranges::count_if(m_Passes, [](const auto& p) { return !p.isCulled; }),
                     std::ranges::count_if(m_Resources, [](const auto& r) { return !r.isCulled; }));
    }

    void Execute(CommandList& cmdList) {
        // Create resources
        for (auto& resource : m_Resources) {
            if (resource.isCulled || resource.isImported) continue;

            if (resource.type == ResourceNode::Buffer) {
                resource.resource = std::make_unique<Buffer>(*m_Device, resource.bufferDesc);
            } else {
                resource.resource = std::make_unique<Texture>(*m_Device, resource.textureDesc);
            }
        }

        // Execute passes
        RenderGraphResources resources(&m_Resources, &cmdList);

        for (U32 passIdx : m_ExecutionOrder) {
            auto& pass = m_Passes[passIdx];
            if (pass.isCulled) continue;

            // Handle resource transitions
            HandleResourceTransitions(cmdList, passIdx);

            // Execute pass
            pass.execute(resources, cmdList);
        }
    }

    std::string GenerateDotGraph() const {
        std::stringstream ss;
        ss << "digraph RenderGraph {\n";
        ss << "  rankdir=TB;\n";
        ss << "  node [shape=box];\n\n";

        // Add resource nodes
        ss << "  // Resources\n";
        for (U32 i = 0; i < m_Resources.size(); ++i) {
            const auto& resource = m_Resources[i];
            if (resource.isCulled) continue;

            std::string shape = resource.type == ResourceNode::Buffer ? "ellipse" : "box";
            std::string color = resource.isImported ? "blue" : "black";
            ss << "  r" << i << " [label=\"" << resource.name << "\", shape=" << shape << ", color=" << color << "];\n";
        }

        // Add pass nodes
        ss << "\n  // Passes\n";
        for (U32 i = 0; i < m_Passes.size(); ++i) {
            const auto& pass = m_Passes[i];
            if (pass.isCulled) continue;

            ss << "  p" << i << " [label=\"" << pass.name << "\", shape=rectangle, style=filled, fillcolor=lightgray];\n";
        }

        // Add edges
        ss << "\n  // Dependencies\n";
        for (U32 i = 0; i < m_Passes.size(); ++i) {
            const auto& pass = m_Passes[i];
            if (pass.isCulled) continue;

            for (auto handle : pass.reads) {
                ss << "  r" << handle.index << " -> p" << i << " [color=green];\n";
            }

            for (auto handle : pass.writes) {
                ss << "  p" << i << " -> r" << handle.index << " [color=red];\n";
            }
        }

        ss << "}\n";
        return ss.str();
    }

private:
    void BuildDependencies() {
        for (U32 passIdx = 0; passIdx < m_Passes.size(); ++passIdx) {
            auto& pass = m_Passes[passIdx];

            // Find dependencies based on resource usage
            for (auto writeHandle : pass.writes) {
                auto& resource = m_Resources[writeHandle.index];

                // This pass depends on all previous readers
                for (U32 readerIdx : resource.readerPasses) {
                    if (readerIdx < passIdx) {
                        pass.dependencies.push_back(readerIdx);
                    }
                }
            }

            for (auto readHandle : pass.reads) {
                auto& resource = m_Resources[readHandle.index];

                // This pass depends on all previous writers
                for (U32 writerIdx : resource.writerPasses) {
                    if (writerIdx < passIdx) {
                        pass.dependencies.push_back(writerIdx);
                    }
                }
            }

            // Remove duplicates
            std::sort(pass.dependencies.begin(), pass.dependencies.end());
            pass.dependencies.erase(std::unique(pass.dependencies.begin(), pass.dependencies.end()), pass.dependencies.end());
        }
    }

    void CullUnusedPasses() {
        // Mark all resources written by the last pass as used
        if (!m_Passes.empty()) {
            auto& lastPass = m_Passes.back();
            lastPass.isCulled = false;

            std::queue<U32> passQueue;
            passQueue.push(static_cast<U32>(m_Passes.size() - 1));

            // Propagate usage backwards
            while (!passQueue.empty()) {
                U32 passIdx = passQueue.front();
                passQueue.pop();

                auto& pass = m_Passes[passIdx];

                // Mark all read resources as used
                for (auto handle : pass.reads) {
                    auto& resource = m_Resources[handle.index];
                    resource.isCulled = false;

                    // Mark writers of this resource as used
                    for (U32 writerIdx : resource.writerPasses) {
                        if (m_Passes[writerIdx].isCulled) {
                            m_Passes[writerIdx].isCulled = false;
                            passQueue.push(writerIdx);
                        }
                    }
                }
            }
        }

        // Cull resources not used by any non-culled pass
        for (auto& resource : m_Resources) {
            if (!resource.isImported && resource.writerPasses.empty() && resource.readerPasses.empty()) {
                resource.isCulled = true;
            }
        }
    }

    void CalculateExecutionOrder() {
        // Topological sort
        Vector<U32> inDegree(m_Passes.size(), 0);

        for (U32 i = 0; i < m_Passes.size(); ++i) {
            if (m_Passes[i].isCulled) continue;

            for (U32 dep : m_Passes[i].dependencies) {
                if (!m_Passes[dep].isCulled) {
                    inDegree[i]++;
                }
            }
        }

        std::queue<U32> queue;
        for (U32 i = 0; i < m_Passes.size(); ++i) {
            if (!m_Passes[i].isCulled && inDegree[i] == 0) {
                queue.push(i);
            }
        }

        m_ExecutionOrder.clear();
        while (!queue.empty()) {
            U32 passIdx = queue.front();
            queue.pop();

            m_ExecutionOrder.push_back(passIdx);
            m_Passes[passIdx].executionOrder = static_cast<U32>(m_ExecutionOrder.size() - 1);

            // Update dependent passes
            for (U32 i = 0; i < m_Passes.size(); ++i) {
                if (m_Passes[i].isCulled) continue;

                auto& deps = m_Passes[i].dependencies;
                if (std::find(deps.begin(), deps.end(), passIdx) != deps.end()) {
                    if (--inDegree[i] == 0) {
                        queue.push(i);
                    }
                }
            }
        }

        assert(m_ExecutionOrder.size() == std::count_if(m_Passes.begin(), m_Passes.end(),
               [](const auto& p) { return !p.isCulled; }), "Cyclic dependency detected");
    }

    void CalculateResourceLifetimes() {
        for (auto& resource : m_Resources) {
            resource.firstPass = INVALID_INDEX;
            resource.lastPass = INVALID_INDEX;

            if (resource.isCulled) continue;

            // Find first and last usage
            for (U32 passIdx : m_ExecutionOrder) {
                auto& pass = m_Passes[passIdx];

                bool used = false;
                for (auto handle : pass.reads) {
                    if (handle.index == static_cast<U32>(&resource - m_Resources.data())) {
                        used = true;
                        break;
                    }
                }

                if (!used) {
                    for (auto handle : pass.writes) {
                        if (handle.index == static_cast<U32>(&resource - m_Resources.data())) {
                            used = true;
                            break;
                        }
                    }
                }

                if (used) {
                    if (resource.firstPass == INVALID_INDEX) {
                        resource.firstPass = pass.executionOrder;
                    }
                    resource.lastPass = pass.executionOrder;
                }
            }
        }
    }

    void HandleResourceTransitions(CommandList& cmdList, U32 passIdx) {
        auto& pass = m_Passes[passIdx];
        Vector<std::pair<Resource*, ::D3D12_RESOURCE_STATES>> transitions;

        for (auto handle : pass.reads) {
            auto& resource = m_Resources[handle.index];
            if (!resource.resource) continue;

            ::D3D12_RESOURCE_STATES targetState = D3D12_RESOURCE_STATE_GENERIC_READ;
            if (resource.type == ResourceNode::Texture) {
                auto& texDesc = resource.textureDesc;
                if (static_cast<U32>(texDesc.usage) & static_cast<U32>(ResourceUsage::DepthStencil)) {
                    targetState = D3D12_RESOURCE_STATE_DEPTH_READ;
                } else {
                    targetState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                }
            }

            transitions.emplace_back(resource.resource.get(), targetState);
        }

        for (auto handle : pass.writes) {
            auto& resource = m_Resources[handle.index];
            if (!resource.resource) continue;

            ::D3D12_RESOURCE_STATES targetState = D3D12_RESOURCE_STATE_COMMON;
            if (resource.type == ResourceNode::Texture) {
                auto& texDesc = resource.textureDesc;
                if (static_cast<U32>(texDesc.usage) & static_cast<U32>(ResourceUsage::RenderTarget)) {
                    targetState = D3D12_RESOURCE_STATE_RENDER_TARGET;
                } else if (static_cast<U32>(texDesc.usage) & static_cast<U32>(ResourceUsage::DepthStencil)) {
                    targetState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
                } else if (static_cast<U32>(texDesc.usage) & static_cast<U32>(ResourceUsage::UnorderedAccess)) {
                    targetState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                }
            } else {
                if (static_cast<U32>(resource.bufferDesc.usage) & static_cast<U32>(ResourceUsage::UnorderedAccess)) {
                    targetState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                }
            }

            transitions.emplace_back(resource.resource.get(), targetState);
        }

        cmdList.TransitionResources(transitions);
    }

    RenderGraphResourceHandle CreateResource(U32 passIndex, const std::string& name, const BufferDesc& desc) {
        U32 index = static_cast<U32>(m_Resources.size());
        m_Resources.emplace_back(name, desc);

        RenderGraphResourceHandle handle;
        handle.index = index;
        handle.version = m_ResourceVersionCounter++;

        return handle;
    }

    RenderGraphResourceHandle CreateResource(U32 passIndex, const std::string& name, const TextureDesc& desc) {
        U32 index = static_cast<U32>(m_Resources.size());
        m_Resources.emplace_back(name, desc);

        RenderGraphResourceHandle handle;
        handle.index = index;
        handle.version = m_ResourceVersionCounter++;

        return handle;
    }
};

// RenderGraphBuilder implementation
RenderGraphResourceHandle RenderGraphBuilder::CreateBuffer(const std::string& name, const BufferDesc& desc) {
    return m_Graph->CreateResource(m_PassIndex, name, desc);
}

RenderGraphResourceHandle RenderGraphBuilder::CreateTexture(const std::string& name, const TextureDesc& desc) {
    return m_Graph->CreateResource(m_PassIndex, name, desc);
}

RenderGraphResourceHandle RenderGraphBuilder::ImportBuffer(const std::string& name, Buffer* buffer) {
    U32 index = static_cast<U32>(m_Graph->m_Resources.size());
    m_Graph->m_Resources.emplace_back(name, buffer->GetDesc());
    m_Graph->m_Resources.back().resource.reset(buffer);
    m_Graph->m_Resources.back().isImported = true;

    RenderGraphResourceHandle handle;
    handle.index = index;
    handle.version = m_Graph->m_ResourceVersionCounter++;

    return handle;
}

RenderGraphResourceHandle RenderGraphBuilder::ImportTexture(const std::string& name, Texture* texture) {
    U32 index = static_cast<U32>(m_Graph->m_Resources.size());
    m_Graph->m_Resources.emplace_back(name, texture->GetDesc());
    m_Graph->m_Resources.back().resource.reset(texture);
    m_Graph->m_Resources.back().isImported = true;

    RenderGraphResourceHandle handle;
    handle.index = index;
    handle.version = m_Graph->m_ResourceVersionCounter++;

    return handle;
}

void RenderGraphBuilder::ReadBuffer(RenderGraphResourceHandle handle) {
    assert(handle.IsValid(), "Invalid resource handle");
    m_Graph->m_Passes[m_PassIndex].reads.push_back(handle);
    m_Graph->m_Resources[handle.index].readerPasses.push_back(m_PassIndex);
}

void RenderGraphBuilder::WriteBuffer(RenderGraphResourceHandle handle) {
    assert(handle.IsValid(), "Invalid resource handle");
    m_Graph->m_Passes[m_PassIndex].writes.push_back(handle);
    m_Graph->m_Resources[handle.index].writerPasses.push_back(m_PassIndex);
}

void RenderGraphBuilder::ReadTexture(RenderGraphResourceHandle handle) {
    assert(handle.IsValid(), "Invalid resource handle");
    m_Graph->m_Passes[m_PassIndex].reads.push_back(handle);
    m_Graph->m_Resources[handle.index].readerPasses.push_back(m_PassIndex);
}

void RenderGraphBuilder::WriteTexture(RenderGraphResourceHandle handle) {
    assert(handle.IsValid(), "Invalid resource handle");
    m_Graph->m_Passes[m_PassIndex].writes.push_back(handle);
    m_Graph->m_Resources[handle.index].writerPasses.push_back(m_PassIndex);
}