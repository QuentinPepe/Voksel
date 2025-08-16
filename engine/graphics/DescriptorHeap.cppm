module;
#include <d3d12.h>
#include <d3dx12.h>
#include <wrl/client.h>

export module Graphics.DX12.DescriptorHeap;

import Core.Assert;
import Core.Types;
import Graphics.DX12.Device;
import std;

using Microsoft::WRL::ComPtr;

export struct DescriptorHandle {
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = {0};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {0};
    bool isValid = false;
    operator bool() const { return isValid; }
};

export class DescriptorHeap {
private:
    ComPtr<ID3D12DescriptorHeap> m_Heap;
    D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
    U32 m_DescriptorSize;
    U32 m_MaxDescriptors;
    U32 m_CurrentOffset = 0;
    bool m_IsShaderVisible;
    D3D12_CPU_DESCRIPTOR_HANDLE m_CpuStart;
    D3D12_GPU_DESCRIPTOR_HANDLE m_GpuStart;

public:
    DescriptorHeap(Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, U32 maxDescriptors, bool shaderVisible = false)
        : m_Type{type}, m_MaxDescriptors{maxDescriptors}, m_IsShaderVisible{shaderVisible} {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = type;
        desc.NumDescriptors = maxDescriptors;
        desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 0;
        assert(SUCCEEDED(device.GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_Heap))), "Failed to create descriptor heap");
        m_DescriptorSize = device.GetDevice()->GetDescriptorHandleIncrementSize(type);
        m_CpuStart = m_Heap->GetCPUDescriptorHandleForHeapStart();
        if (shaderVisible) {
            m_GpuStart = m_Heap->GetGPUDescriptorHandleForHeapStart();
        }
    }

    DescriptorHandle Allocate() {
        assert(m_CurrentOffset < m_MaxDescriptors, "Descriptor heap is full");
        DescriptorHandle handle;
        handle.cpuHandle.ptr = m_CpuStart.ptr + static_cast<SIZE_T>(m_CurrentOffset) * m_DescriptorSize;
        if (m_IsShaderVisible) {
            handle.gpuHandle.ptr = m_GpuStart.ptr + static_cast<UINT64>(m_CurrentOffset) * m_DescriptorSize;
        }
        handle.isValid = true;
        m_CurrentOffset++;
        return handle;
    }

    void Reset() {
        m_CurrentOffset = 0;
    }

    [[nodiscard]] ID3D12DescriptorHeap* GetHeap() const { return m_Heap.Get(); }
    [[nodiscard]] U32 GetDescriptorSize() const { return m_DescriptorSize; }
    [[nodiscard]] bool IsShaderVisible() const { return m_IsShaderVisible; }
    [[nodiscard]] bool HasSpace() const { return m_CurrentOffset < m_MaxDescriptors; }
};

export class DynamicDescriptorHeap {
private:
    static constexpr U32 DESCRIPTORS_PER_HEAP = 1024;
    Device* m_Device;
    D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
    Vector<std::unique_ptr<DescriptorHeap>> m_Heaps;
    U32 m_CurrentHeapIndex = 0;

public:
    DynamicDescriptorHeap(Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type)
        : m_Device{&device}, m_Type{type} {
        AllocateNewHeap();
    }

    DescriptorHandle Allocate() {
        auto& currentHeap = m_Heaps[m_CurrentHeapIndex];
        if (!currentHeap->HasSpace()) {
            AllocateNewHeap();
        }
        return m_Heaps[m_CurrentHeapIndex]->Allocate();
    }

    void Reset() {
        for (auto& heap : m_Heaps) {
            heap->Reset();
        }
        m_CurrentHeapIndex = 0;
    }

    [[nodiscard]] ID3D12DescriptorHeap* GetCurrentHeap() const {
        return m_Heaps[m_CurrentHeapIndex]->GetHeap();
    }

private:
    void AllocateNewHeap() {
        bool shaderVisible = (m_Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
                             m_Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        m_Heaps.emplace_back(std::make_unique<DescriptorHeap>(*m_Device, m_Type, DESCRIPTORS_PER_HEAP, shaderVisible));
        m_CurrentHeapIndex = static_cast<U32>(m_Heaps.size() - 1);
    }
};
