module;
#include <wincodec.h>
#include <wrl/client.h>
export module Systems.VoxelRenderer;

import ECS.SystemScheduler;
import ECS.Query;
import ECS.World;
import Components.Voxel;
import Components.Camera;
import Components.Transform;
import Systems.CameraManager;
import Graphics;
import Graphics.RenderData;
import Core.Types;
import Core.Assert;
import Math.Matrix;
import std;

using Microsoft::WRL::ComPtr;

static bool LoadImageRGBA(const std::filesystem::path& path, Vector<U32>& out, U32& w, U32& h) {
    static ComPtr<IWICImagingFactory> factory;
    if (!factory) {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return false;
    }
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(path.wstring().c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))) return false;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return false;
    UINT fw = 0, fh = 0;
    if (FAILED(frame->GetSize(&fw, &fh))) return false;
    ComPtr<IWICFormatConverter> conv;
    if (FAILED(factory->CreateFormatConverter(&conv))) return false;
    if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return false;
    out.resize(static_cast<USize>(fw) * fh);
    if (FAILED(conv->CopyPixels(nullptr, fw * 4, static_cast<UINT>(out.size() * 4), reinterpret_cast<BYTE*>(out.data())))) return false;
    w = fw; h = fh;
    return true;
}

static void BlitWithPad(Vector<U32>& dst, U32 atlasW, U32 atlasH, U32 tx, U32 ty, U32 tile, U32 pad, const Vector<U32>& src) {
    U32 ox{tx * (tile + 2 * pad)};
    U32 oy{ty * (tile + 2 * pad)};
    U32 sx{ox + pad};
    U32 sy{oy + pad};

    for (U32 y{0}; y < tile; ++y) for (U32 x{0}; x < tile; ++x)
        dst[(sy + y) * atlasW + (sx + x)] = src[y * tile + x];

    for (U32 x{ox}; x < ox + (2 * pad + tile); ++x) {
        U32 cx{std::clamp<U32>(x, sx, sx + tile - 1)};
        for (U32 y{0}; y < pad; ++y) {
            dst[(sy - 1 - y) * atlasW + x] = dst[sy * atlasW + cx];
            dst[(sy + tile + y) * atlasW + x] = dst[(sy + tile - 1) * atlasW + cx];
        }
    }

    for (U32 y{oy}; y < oy + (2 * pad + tile); ++y) {
        U32 cy{std::clamp<U32>(y, sy, sy + tile - 1)};
        for (U32 x{0}; x < pad; ++x) {
            dst[y * atlasW + (sx - 1 - x)] = dst[cy * atlasW + sx];
            dst[y * atlasW + (sx + tile + x)] = dst[cy * atlasW + (sx + tile - 1)];
        }
    }
}

export class VoxelRendererSystem : public System<VoxelRendererSystem> {
private:
    IGraphicsContext* m_Gfx{nullptr};
    U32 m_CameraCB{INVALID_INDEX};
    U32 m_ObjectCB{INVALID_INDEX};
    U32 m_AtlasCB{INVALID_INDEX};
    U32 m_AtlasTex{INVALID_INDEX};

public:
    void Setup() {
        SetName("VoxelRenderer");
        SetStage(SystemStage::Render);
        SetPriority(SystemPriority::Normal);
        SetParallel(false);
    }

    void SetGraphicsContext(IGraphicsContext* gfx) { m_Gfx = gfx; }

    U32 CreatePipeline() const {
        ShaderCode vs{}; vs.source = "voxel\\Voxel.hlsl"; vs.entryPoint = "VSMain"; vs.stage = ShaderStage::Vertex;
        ShaderCode ps{}; ps.source = "voxel\\Voxel.hlsl"; ps.entryPoint = "PSMain"; ps.stage = ShaderStage::Pixel;
        GraphicsPipelineCreateInfo pi{};
        pi.shaders = {vs, ps};
        pi.vertexAttributes = {{"POSITION", 0}, {"NORMAL", 12}, {"TEXCOORD", 24}, {"COLOR", 32}};
        pi.vertexStride = sizeof(Vertex);
        pi.topology = PrimitiveTopology::TriangleList;
        pi.depthTest = true;
        pi.depthWrite = true;
        return m_Gfx->CreateGraphicsPipeline(pi);
    }

    void Run(World* world, F32) override {
        assert(m_Gfx != nullptr, "GraphicsContext must be set");
        auto* rr{world->GetStorage<VoxelRenderResources>()};
        if (!rr || rr->Size() == 0) {
            auto e{world->CreateEntity()};
            VoxelRenderResources r{}; r.pipeline = CreatePipeline();
            world->AddComponent(e, r);
            rr = world->GetStorage<VoxelRenderResources>();
        }
        const VoxelRenderResources* res{}; for (auto [h, r] : *rr) { res = &r; break; }
        assert(res != nullptr && res->pipeline != INVALID_INDEX, "Invalid render resources");
        if (m_CameraCB == INVALID_INDEX) m_CameraCB = m_Gfx->CreateConstantBuffer(sizeof(CameraConstants));
        if (m_ObjectCB  == INVALID_INDEX) m_ObjectCB  = m_Gfx->CreateConstantBuffer(sizeof(ObjectConstants));
        if (m_AtlasCB   == INVALID_INDEX) m_AtlasCB   = m_Gfx->CreateConstantBuffer(sizeof(AtlasConstants));
        if (m_AtlasTex == INVALID_INDEX) {
            const U32 pad = 2;
            Vector<U32> dirt; U32 dw=0, dh=0;
            Vector<U32> grass; U32 gw=0, gh=0;
            Vector<U32> stone; U32 sw=0, sh=0;
            bool ok0 = LoadImageRGBA(std::filesystem::path("assets/dirt.png"), dirt, dw, dh);
            bool ok1 = LoadImageRGBA(std::filesystem::path("assets/grass.png"), grass, gw, gh);
            bool ok2 = LoadImageRGBA(std::filesystem::path("assets/stone.png"), stone, sw, sh);
            assert(ok0 && ok1 && ok2, "Failed to load atlas tiles");
            assert(dw == dh && gw == gh && sw == sh && dw == gw && dw == sw, "Mismatched tile sizes");
            const U32 tile = dw;
            const U32 tilesX = 3;
            const U32 tilesY = 1;
            const U32 atlasW = tilesX * (tile + 2 * pad);
            const U32 atlasH = tilesY * (tile + 2 * pad);
            Vector<U32> pixels; pixels.resize(static_cast<USize>(atlasW * atlasH), 0u);
            BlitWithPad(pixels, atlasW, atlasH, 0, 0, tile, pad, dirt);
            BlitWithPad(pixels, atlasW, atlasH, 1, 0, tile, pad, grass);
            BlitWithPad(pixels, atlasW, atlasH, 2, 0, tile, pad, stone);
            m_AtlasTex = m_Gfx->CreateTexture2D(pixels.data(), atlasW, atlasH, 1);
            AtlasConstants ac{};
            ac.tilesX = tilesX;
            ac.tileSize = tile;
            ac.pad = pad;
            ac._pad0 = 0;
            ac.atlasW = atlasW;
            ac.atlasH = atlasH;
            m_Gfx->UpdateConstantBuffer(m_AtlasCB, &ac, sizeof(ac));
        }
        CameraConstants cam{};
        if (auto h = CameraManager::GetPrimaryCamera(); h.valid()) {
            if (auto* camCmp = world->GetComponent<Camera>(h)) {
                cam.view           = camCmp->view;
                cam.projection     = camCmp->projection;
                cam.viewProjection = camCmp->viewProjection;
            }
            if (auto* tr = world->GetComponent<Transform>(h)) {
                cam.cameraPosition = tr->position;
            }
        }
        m_Gfx->UpdateConstantBuffer(m_CameraCB, &cam, sizeof(cam));
        ObjectConstants obj{};
        obj.world = Math::Mat4::Identity;
        m_Gfx->UpdateConstantBuffer(m_ObjectCB, &obj, sizeof(obj));
        auto* storage{world->GetStorage<VoxelMesh>()};
        if (!storage) { return; }
        m_Gfx->SetPipeline(res->pipeline);
        m_Gfx->SetConstantBuffer(m_CameraCB, 0);
        m_Gfx->SetConstantBuffer(m_ObjectCB,  1);
        m_Gfx->SetTexture(m_AtlasTex, 0);
        m_Gfx->SetConstantBuffer(m_AtlasCB, 2);
        for (auto [handle, mesh] : *storage) {
            if (mesh.vertexBuffer == INVALID_INDEX || mesh.vertexCount == 0) { continue; }
            m_Gfx->SetVertexBuffer(mesh.vertexBuffer);
            m_Gfx->Draw(mesh.vertexCount);
        }
    }
};
