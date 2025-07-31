module;
#include <d3d12.h>
#include <d3dx12.h>
#include <wrl/client.h>

export module Graphics.DX12.CommandList;

import Core.Assert;
import Core.Types;
import Graphics.DX12.Device;
import Graphics.DX12.Resource;
import std;

using Microsoft::WRL::ComPtr;

export class CommandList {
private:
    ComPtr<ID3D12CommandAllocator> m_Allocator;
    ComPtr<ID3D12GraphicsCommandList6> m_CommandList;
    D3D12_COMMAND_LIST_TYPE m_Type;
    bool m_IsRecording = false;

public:
    CommandList(Device& device, D3D12_COMMAND_LIST_TYPE type)
        : m_Type{type} {

        assert(SUCCEEDED(device.GetDevice()->CreateCommandAllocator(type, IID_PPV_ARGS(&m_Allocator))),
               "Failed to create command allocator");

        assert(SUCCEEDED(device.GetDevice()->CreateCommandList(
            0,
            type,
            m_Allocator.Get(),
            nullptr,
            IID_PPV_ARGS(&m_CommandList)
        )), "Failed to create command list");

        // Command lists are created in recording state, close it
        m_CommandList->Close();
    }

    void Begin() {
        assert(!m_IsRecording, "Command list is already recording");
        assert(SUCCEEDED(m_Allocator->Reset()), "Failed to reset command allocator");
        assert(SUCCEEDED(m_CommandList->Reset(m_Allocator.Get(), nullptr)), "Failed to reset command list");
        m_IsRecording = true;
    }

    void End() {
        assert(m_IsRecording, "Command list is not recording");
        assert(SUCCEEDED(m_CommandList->Close()), "Failed to close command list");
        m_IsRecording = false;
    }

    void TransitionResource(Resource& resource, D3D12_RESOURCE_STATES newState) const {
        if (!resource.IsTracked() || resource.GetCurrentState() == newState) {
            return;
        }

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource.GetResource();
        barrier.Transition.StateBefore = resource.GetCurrentState();
        barrier.Transition.StateAfter = newState;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        m_CommandList->ResourceBarrier(1, &barrier);
        resource.SetCurrentState(newState);
    }

    void TransitionResources(std::span<std::pair<Resource*, D3D12_RESOURCE_STATES>> transitions) const {
        Vector<D3D12_RESOURCE_BARRIER> barriers;
        barriers.reserve(transitions.size());

        for (auto& [resource, newState] : transitions) {
            if (!resource->IsTracked() || resource->GetCurrentState() == newState) {
                continue;
            }

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = resource->GetResource();
            barrier.Transition.StateBefore = resource->GetCurrentState();
            barrier.Transition.StateAfter = newState;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            barriers.push_back(barrier);
            resource->SetCurrentState(newState);
        }

        if (!barriers.empty()) {
            m_CommandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
        }
    }

    void CopyResource(ID3D12Resource* dst, ID3D12Resource* src) const {
        m_CommandList->CopyResource(dst, src);
    }

    void CopyBufferRegion(ID3D12Resource* dst, U64 dstOffset, ID3D12Resource* src, U64 srcOffset, U64 size) {
        m_CommandList->CopyBufferRegion(dst, dstOffset, src, srcOffset, size);
    }

    void SetDescriptorHeaps(U32 count, ID3D12DescriptorHeap* const* heaps) {
        m_CommandList->SetDescriptorHeaps(count, heaps);
    }

    [[nodiscard]] ID3D12GraphicsCommandList6* GetCommandList() const { return m_CommandList.Get(); }
    [[nodiscard]] D3D12_COMMAND_LIST_TYPE GetType() const { return m_Type; }
    [[nodiscard]] bool IsRecording() const { return m_IsRecording; }
};