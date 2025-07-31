module;
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dx12.h>
#include <wrl/client.h>

export module Graphics.DX12.Device;

import Core.Assert;
import Core.Types;
import Core.Log;
import Graphics.Window;

using Microsoft::WRL::ComPtr;

export struct DeviceConfig {
    bool enableDebugLayer = true;
    bool enableGPUValidation = false;
};

export class Device {
private:
    ComPtr<ID3D12Device8> m_Device;
    ComPtr<IDXGIFactory7> m_Factory;
    ComPtr<IDXGIAdapter4> m_Adapter;

    struct QueueInfo {
        ComPtr<ID3D12CommandQueue> queue;
        D3D12_COMMAND_LIST_TYPE type;
        U32 familyIndex;
    };

    QueueInfo m_DirectQueue;
    QueueInfo m_ComputeQueue;
    QueueInfo m_CopyQueue;

public:
    explicit Device(const DeviceConfig& config) {
        if (config.enableDebugLayer) {
            ComPtr<ID3D12Debug3> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
                debugController->EnableDebugLayer();
                if (config.enableGPUValidation) {
                    debugController->SetEnableGPUBasedValidation(TRUE);
                }
                Logger::Info("D3D12 Debug Layer enabled");
            } else {
                Logger::Warn("Failed to enable D3D12 Debug Layer");
            }
        }

        UINT dxgiFactoryFlags = 0;
        if (config.enableDebugLayer) {
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }

        HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_Factory));
        assert(SUCCEEDED(hr), "Failed to create DXGI factory");

        // Find best adapter
        ComPtr<IDXGIAdapter1> adapter;
        ComPtr<IDXGIAdapter4> bestAdapter;
        SIZE_T maxDedicatedVideoMemory = 0;
        bool foundCompatibleAdapter = false;

        Logger::Info("Enumerating GPU adapters...");

        for (UINT i = 0; m_Factory->EnumAdapterByGpuPreference(
            i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; ++i) {

            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            // Convert wide string to narrow string for logging
            char adapterName[128];
            size_t convertedChars = 0;
            wcstombs_s(&convertedChars, adapterName, desc.Description, sizeof(adapterName));

            Logger::Info("Found adapter {}: {}", i, adapterName);

            // Skip software adapters
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                Logger::Info("  Skipping software adapter");
                continue;
            }

            // Try to create a device with D3D12
            ComPtr<ID3D12Device> testDevice;
            hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&testDevice));

            if (SUCCEEDED(hr)) {
                Logger::Info("  D3D12 compatible, VRAM: {} MB", desc.DedicatedVideoMemory / (1024 * 1024));
                foundCompatibleAdapter = true;

                if (desc.DedicatedVideoMemory > maxDedicatedVideoMemory) {
                    maxDedicatedVideoMemory = desc.DedicatedVideoMemory;
                    adapter.As(&bestAdapter);
                }
            } else {
                Logger::Info("  Not D3D12 compatible");
            }
        }

        if (!foundCompatibleAdapter) {
            Logger::Error("No D3D12-compatible GPU found. Please ensure:");
            Logger::Error("  - You have a DirectX 12 compatible GPU");
            Logger::Error("  - Your GPU drivers are up to date");
            Logger::Error("  - Windows 10 version 1903 or later is installed");
        }

        assert(bestAdapter, "No suitable GPU adapter found");
        m_Adapter = bestAdapter;

        // Create the actual device
        hr = D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_Device));
        if (FAILED(hr)) {
            Logger::Error("Failed to create D3D12 device. HRESULT: 0x{:08X}", hr);
            assert(false, "Failed to create D3D12 device");
        }

        // Log selected adapter info
        DXGI_ADAPTER_DESC1 finalDesc;
        m_Adapter->GetDesc1(&finalDesc);
        char finalAdapterName[128];
        size_t finalConvertedChars = 0;
        wcstombs_s(&finalConvertedChars, finalAdapterName, finalDesc.Description, sizeof(finalAdapterName));
        Logger::Info("Using GPU: {}", finalAdapterName);

        // Create command queues
        CreateQueue(m_DirectQueue, D3D12_COMMAND_LIST_TYPE_DIRECT);
        CreateQueue(m_ComputeQueue, D3D12_COMMAND_LIST_TYPE_COMPUTE);
        CreateQueue(m_CopyQueue, D3D12_COMMAND_LIST_TYPE_COPY);

        Logger::Info("D3D12 Device created successfully");
    }

    ~Device() {}

    [[nodiscard]] ID3D12Device8* GetDevice() const { return m_Device.Get(); }
    [[nodiscard]] IDXGIFactory7* GetFactory() const { return m_Factory.Get(); }
    [[nodiscard]] ID3D12CommandQueue* GetDirectQueue() const { return m_DirectQueue.queue.Get(); }
    [[nodiscard]] ID3D12CommandQueue* GetComputeQueue() const { return m_ComputeQueue.queue.Get(); }
    [[nodiscard]] ID3D12CommandQueue* GetCopyQueue() const { return m_CopyQueue.queue.Get(); }

private:
    void CreateQueue(QueueInfo& info, D3D12_COMMAND_LIST_TYPE type) const {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = type;
        queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.NodeMask = 0;

        HRESULT hr = m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&info.queue));
        assert(SUCCEEDED(hr), "Failed to create command queue");
        info.type = type;
    }
};