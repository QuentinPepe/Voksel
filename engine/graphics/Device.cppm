module;

#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <windows.h>
#include <wrl/client.h>
#include <vector>
#include <string>

export module Graphics.DX12.Device;

import Core.Types;
import Core.Log;

using Microsoft::WRL::ComPtr;

export struct DeviceConfig {
    bool enableDebugLayer = true;
};

export class Device {
public:
    Device() : Device{DeviceConfig{}} {}

    explicit Device(const DeviceConfig& config) {
        InitializeFactory(config);
        SelectAdapter();
        CreateDevice();
        CreateCommandQueues();
        QueryFeatures();
    }

    ~Device() {
        if (m_DirectQueue) {
            ID3D12Fence *fence = nullptr;
            UINT64 fenceValue = 0;

            if (SUCCEEDED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) {
                fenceValue = 1;
                m_DirectQueue->Signal(fence, fenceValue);

                if (fence->GetCompletedValue() < fenceValue) {
                    HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
                    if (eventHandle) {
                        fence->SetEventOnCompletion(fenceValue, eventHandle);
                        WaitForSingleObject(eventHandle, INFINITE);
                        CloseHandle(eventHandle);
                    }
                }
                fence->Release();
            }
        }

        if (m_DirectQueue) {
            m_DirectQueue->Release();
            m_DirectQueue = nullptr;
        }

        if (m_ComputeQueue) {
            m_ComputeQueue->Release();
            m_ComputeQueue = nullptr;
        }

        if (m_CopyQueue) {
            m_CopyQueue->Release();
            m_CopyQueue = nullptr;
        }

        if (m_Adapter) {
            m_Adapter->Release();
            m_Adapter = nullptr;
        }

        if (m_Device) {
            m_Device->Release();
            m_Device = nullptr;
        }

        if (m_Factory) {
            m_Factory->Release();
            m_Factory = nullptr;
        }

#ifdef _DEBUG
        {
            IDXGIDebug1 *dxgiDebug = nullptr;
            if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
                dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL,
                                             DXGI_DEBUG_RLO_FLAGS(
                                                 DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
                dxgiDebug->Release();
            }
        }
#endif
    }

    ID3D12Device8 *GetDevice() const { return m_Device; }
    IDXGIFactory6 *GetFactory() const { return m_Factory; }
    IDXGIAdapter4 *GetAdapter() const { return m_Adapter; }

    ID3D12CommandQueue *GetDirectQueue() const { return m_DirectQueue; }
    ID3D12CommandQueue *GetComputeQueue() const { return m_ComputeQueue; }
    ID3D12CommandQueue *GetCopyQueue() const { return m_CopyQueue; }

    void SetCommandQueue(ID3D12CommandQueue *queue) { m_DirectQueue = queue; }

    bool SupportsRayTracing() const { return m_Features.rayTracing; }
    bool SupportsMeshShaders() const { return m_Features.meshShaders; }
    bool SupportsVariableRateShading() const { return m_Features.variableRateShading; }

private:
    void InitializeFactory(const DeviceConfig& /*config*/) {
        UINT dxgiFactoryFlags = 0;

#ifdef _DEBUG
        if (config.enableDebugLayer) {
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
                debugController->EnableDebugLayer();
                dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }
#endif

        HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_Factory));
        if (FAILED(hr)) {
            Logger::Error("Failed to create DXGI factory");
        }
    }

    void SelectAdapter() {
        IDXGIAdapter1 *adapter = nullptr;

        for (UINT i = 0; m_Factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                               IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                adapter->Release();
                continue;
            }

            if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr))) {
                adapter->QueryInterface(IID_PPV_ARGS(&m_Adapter));
                break;
            }

            adapter->Release();
        }

        if (!m_Adapter) {
            Logger::Error("No suitable GPU adapter found");
        }

        DXGI_ADAPTER_DESC1 desc;
        m_Adapter->GetDesc1(&desc);

        std::wstring wName(desc.Description);
        std::string name;
        name.reserve(wName.size());
        for (wchar_t wc : wName) {
            name.push_back(static_cast<char>(wc));
        }
        Logger::Info("Selected GPU: {}", name);
    }

    void CreateDevice() {
        HRESULT hr = D3D12CreateDevice(m_Adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_Device));
        if (FAILED(hr)) {
            Logger::Error("Failed to create D3D12 device");
        }

#ifdef _DEBUG
        ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(m_Device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

            D3D12_MESSAGE_SEVERITY severities[] = {D3D12_MESSAGE_SEVERITY_INFO};
            D3D12_MESSAGE_ID denyIds[] = {
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
            };

            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumSeverities = _countof(severities);
            filter.DenyList.pSeverityList = severities;
            filter.DenyList.NumIDs = _countof(denyIds);
            filter.DenyList.pIDList = denyIds;

            infoQueue->PushStorageFilter(&filter);
        }
#endif
    }

    void CreateCommandQueues() {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.NodeMask = 0;

        HRESULT hr = m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_DirectQueue));
        if (FAILED(hr)) {
            Logger::Error("Failed to create direct command queue");
        }

        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        hr = m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_ComputeQueue));
        if (FAILED(hr)) {
            Logger::Error("Failed to create compute command queue");
        }

        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        hr = m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CopyQueue));
        if (FAILED(hr)) {
            Logger::Error("Failed to create copy command queue");
        }
    }

    void QueryFeatures() {
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
        if (SUCCEEDED(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)))) {
            m_Features.rayTracing = options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
        }

        D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
        if (SUCCEEDED(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7)))) {
            m_Features.meshShaders = options7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
        }

        D3D12_FEATURE_DATA_D3D12_OPTIONS6 options6 = {};
        if (SUCCEEDED(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &options6, sizeof(options6)))) {
            m_Features.variableRateShading = options6.VariableShadingRateTier !=
                                             D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED;
        }

        Logger::Info("GPU Features - RT: {}, Mesh Shaders: {}, VRS: {}",
                     m_Features.rayTracing, m_Features.meshShaders, m_Features.variableRateShading);
    }

private:
    ID3D12Device8 *m_Device = nullptr;
    IDXGIFactory6 *m_Factory = nullptr;
    IDXGIAdapter4 *m_Adapter = nullptr;
    ID3D12CommandQueue *m_DirectQueue = nullptr;
    ID3D12CommandQueue *m_ComputeQueue = nullptr;
    ID3D12CommandQueue *m_CopyQueue = nullptr;

    struct {
        bool rayTracing = false;
        bool meshShaders = false;
        bool variableRateShading = false;
    } m_Features;
};