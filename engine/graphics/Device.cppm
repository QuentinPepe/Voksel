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
import Core.Assert;

using Microsoft::WRL::ComPtr;

export struct DeviceConfig {
    bool enableDebugLayer{true};
};

export class Device {
public:
    Device() : Device{DeviceConfig{}} {
    }

    explicit Device(DeviceConfig const &config) {
        InitializeFactory(config);
        SelectAdapter();
        CreateDevice();
        CreateCommandQueues();
        QueryFeatures();
    }

    ~Device() {
        if (m_DirectQueue && m_Device) {
            Microsoft::WRL::ComPtr<ID3D12Fence> fence{};
            if (SUCCEEDED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) {
                HANDLE evt{CreateEvent(nullptr, FALSE, FALSE, nullptr)};
                if (evt) {
                    if (SUCCEEDED(m_DirectQueue->Signal(fence.Get(), 1))) {
                        if (fence->GetCompletedValue() < 1) {
                            fence->SetEventOnCompletion(1, evt);
                            WaitForSingleObject(evt, INFINITE);
                        }
                    }
                    CloseHandle(evt);
                }
            }
        }
        m_CopyQueue.Reset();
        m_ComputeQueue.Reset();
        m_DirectQueue.Reset();
        m_Adapter.Reset();
        m_Device.Reset();
        m_Factory.Reset();
#ifdef _DEBUG
        Microsoft::WRL::ComPtr<IDXGIDebug1> dbg{};
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dbg)))) {
            dbg->ReportLiveObjects(DXGI_DEBUG_ALL,
                DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        }
#endif
    }

    ID3D12Device8 *GetDevice() const { return m_Device.Get(); }
    IDXGIFactory6 *GetFactory() const { return m_Factory.Get(); }
    IDXGIAdapter4 *GetAdapter() const { return m_Adapter.Get(); }

    ID3D12CommandQueue *GetDirectQueue() const { return m_DirectQueue.Get(); }
    ID3D12CommandQueue *GetComputeQueue() const { return m_ComputeQueue.Get(); }
    ID3D12CommandQueue *GetCopyQueue() const { return m_CopyQueue.Get(); }

    void SetCommandQueue(ID3D12CommandQueue* queue) {
        if (queue) { queue->AddRef(); }
        m_DirectQueue.Reset();
        m_DirectQueue.Attach(queue);
    }

    bool SupportsRayTracing() const { return m_Features.rayTracing; }
    bool SupportsMeshShaders() const { return m_Features.meshShaders; }
    bool SupportsVariableRateShading() const { return m_Features.variableRateShading; }

private:
    void InitializeFactory(DeviceConfig const &config) {
        UINT flags{0};
#ifdef _DEBUG
        if (config.enableDebugLayer) {
            ComPtr<ID3D12Debug> debugController{};
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())))) {
                debugController->EnableDebugLayer();
                flags |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }
#endif
        assert(SUCCEEDED(CreateDXGIFactory2(flags, IID_PPV_ARGS(m_Factory.GetAddressOf()))),
               "CreateDXGIFactory2 failed");
    }

    void SelectAdapter() {
        for (UINT i{0};; ++i) {
            ComPtr<IDXGIAdapter1> tmp{};
            HRESULT hr{
                m_Factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                      IID_PPV_ARGS(tmp.GetAddressOf()))
            };
            if (hr == DXGI_ERROR_NOT_FOUND) break;

            DXGI_ADAPTER_DESC1 desc{};
            tmp->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

            if (SUCCEEDED(D3D12CreateDevice(tmp.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr))) {
                assert(SUCCEEDED(tmp.As(&m_Adapter)), "IDXGIAdapter4 query failed");
                break;
            }
        }

        assert(static_cast<bool>(m_Adapter), "No suitable GPU adapter found");

        DXGI_ADAPTER_DESC1 desc{};
        m_Adapter->GetDesc1(&desc);
        std::wstring wName{desc.Description};
        std::string name{};
        name.reserve(wName.size());
        for (wchar_t wc: wName) name.push_back(static_cast<char>(wc));
        Logger::Info(LogGraphics, "Selected GPU: {}", name);
    }

    void CreateDevice() {
        assert(SUCCEEDED(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(m_Device.GetAddressOf()))),
               "D3D12CreateDevice failed");
#ifdef _DEBUG
        ComPtr<ID3D12InfoQueue> infoQueue{};
        if (SUCCEEDED(m_Device->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);
            D3D12_MESSAGE_SEVERITY severities[]{D3D12_MESSAGE_SEVERITY_INFO};
            D3D12_MESSAGE_ID denyIds[]{
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
                D3D12_MESSAGE_ID_LIVE_DEVICE
            };
            D3D12_INFO_QUEUE_FILTER filter{};
            filter.DenyList.NumSeverities = static_cast<UINT>(_countof(severities));
            filter.DenyList.pSeverityList = severities;
            filter.DenyList.NumIDs = static_cast<UINT>(_countof(denyIds));
            filter.DenyList.pIDList = denyIds;
            infoQueue->PushStorageFilter(&filter);
        }
#endif
    }

    void CreateCommandQueues() {
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        qd.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        qd.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        qd.NodeMask = 0;
        assert(SUCCEEDED(m_Device->CreateCommandQueue(&qd, IID_PPV_ARGS(m_DirectQueue.GetAddressOf()))),
               "Create direct queue failed");

        qd.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        assert(SUCCEEDED(m_Device->CreateCommandQueue(&qd, IID_PPV_ARGS(m_ComputeQueue.GetAddressOf()))),
               "Create compute queue failed");

        qd.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        assert(SUCCEEDED(m_Device->CreateCommandQueue(&qd, IID_PPV_ARGS(m_CopyQueue.GetAddressOf()))),
               "Create copy queue failed");
    }

    void QueryFeatures() {
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 o5{};
        if (SUCCEEDED(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &o5, sizeof(o5)))) {
            m_Features.rayTracing = o5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
        }
        D3D12_FEATURE_DATA_D3D12_OPTIONS7 o7{};
        if (SUCCEEDED(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &o7, sizeof(o7)))) {
            m_Features.meshShaders = o7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
        }
        D3D12_FEATURE_DATA_D3D12_OPTIONS6 o6{};
        if (SUCCEEDED(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &o6, sizeof(o6)))) {
            m_Features.variableRateShading = o6.VariableShadingRateTier !=
                                             D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED;
        }
        Logger::Info(LogGraphics, "GPU Features - RT: {}, Mesh Shaders: {}, VRS: {}",
                     m_Features.rayTracing, m_Features.meshShaders, m_Features.variableRateShading);
    }

private:
    ComPtr<ID3D12Device8> m_Device{};
    ComPtr<IDXGIFactory6> m_Factory{};
    ComPtr<IDXGIAdapter4> m_Adapter{};
    ComPtr<ID3D12CommandQueue> m_DirectQueue{};
    ComPtr<ID3D12CommandQueue> m_ComputeQueue{};
    ComPtr<ID3D12CommandQueue> m_CopyQueue{};

    struct {
        bool rayTracing{false};
        bool meshShaders{false};
        bool variableRateShading{false};
    } m_Features;
};
