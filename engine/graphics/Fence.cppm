module;
#include <d3d12.h>
#include <wrl/client.h>
#include <Windows.h>

export module Graphics.DX12.Fence;

import Core.Assert;
import Core.Types;
import Graphics.DX12.Device;

using Microsoft::WRL::ComPtr;

export class Fence {
private:
    ComPtr<ID3D12Fence> m_Fence;
    U64 m_NextValue = 1;
    HANDLE m_Event = nullptr;

public:
    explicit Fence(Device& device) {
        assert(SUCCEEDED(device.GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence))),
               "Failed to create fence");

        m_Event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        assert(m_Event != nullptr, "Failed to create fence event");
    }

    ~Fence() {
        if (m_Event) {
            CloseHandle(m_Event);
        }
    }

    U64 Signal(ID3D12CommandQueue* queue) {
        U64 value = m_NextValue++;
        assert(SUCCEEDED(queue->Signal(m_Fence.Get(), value)), "Failed to signal fence");
        return value;
    }

    void WaitCPU(U64 value) const {
        if (m_Fence->GetCompletedValue() >= value) { return; }
        const HRESULT hr{m_Fence->SetEventOnCompletion(value, m_Event)};
        if (FAILED(hr) || !m_Event) {
            // Fallback if device is removed during shutdown
            for (;;) {
                if (m_Fence->GetCompletedValue() >= value) { break; }
                ::Sleep(0);
            }
            return;
        }
        WaitForSingleObject(m_Event, INFINITE);
    }

    void WaitGPU(ID3D12CommandQueue* queue, U64 value) const {
        assert(SUCCEEDED(queue->Wait(m_Fence.Get(), value)), "Failed to wait on fence");
    }

    [[nodiscard]] U64 GetCompletedValue() const {
        return m_Fence->GetCompletedValue();
    }

    [[nodiscard]] U64 GetNextValue() const { return m_NextValue; }
    [[nodiscard]] ID3D12Fence* GetFence() const { return m_Fence.Get(); }
};