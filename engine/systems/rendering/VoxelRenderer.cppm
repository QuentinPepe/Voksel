module;
#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>
#include <algorithm>

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
import Math.Vector;
import Math.Matrix;
import Math.Transform;
import std;

using Microsoft::WRL::ComPtr;

static bool LoadImageRGBA(std::filesystem::path const &path, Vector<U32> &out, U32 &w, U32 &h) {
    HRESULT cohr{CoInitializeEx(nullptr, COINIT_MULTITHREADED)};
    bool doUninit{SUCCEEDED(cohr)};
    if (cohr == RPC_E_CHANGED_MODE) doUninit = false;

    ComPtr<IWICImagingFactory> factory{};
    HRESULT hr{
        CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf()))
    };
    if (FAILED(hr)) {
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(factory.GetAddressOf()));
    }
    if (FAILED(hr)) {
        if (doUninit) CoUninitialize();
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder{};
    std::wstring wpath{path.wstring()};
    hr = factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
                                            decoder.GetAddressOf());
    if (FAILED(hr)) {
        if (doUninit) CoUninitialize();
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> frame{};
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) {
        if (doUninit) CoUninitialize();
        return false;
    }

    ComPtr<IWICFormatConverter> conv{};
    hr = factory->CreateFormatConverter(conv.GetAddressOf());
    if (FAILED(hr)) {
        if (doUninit) CoUninitialize();
        return false;
    }

    hr = conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
                          WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        if (doUninit) CoUninitialize();
        return false;
    }

    UINT fw{0}, fh{0};
    hr = conv->GetSize(&fw, &fh);
    if (FAILED(hr) || fw == 0 || fh == 0) {
        if (doUninit) CoUninitialize();
        return false;
    }

    w = fw;
    h = fh;
    out.resize(static_cast<USize>(fw) * fh);
    hr = conv->CopyPixels(nullptr, fw * 4u, static_cast<UINT>(out.size() * 4u), reinterpret_cast<BYTE *>(out.data()));
    if (FAILED(hr)) {
        out.clear();
        if (doUninit) CoUninitialize();
        return false;
    }

    if (doUninit) CoUninitialize();
    return true;
}

static void BlitWithPad(Vector<U32> &dst, U32 atlasW, U32 atlasH, U32 tx, U32 ty, U32 tile, U32 pad,
                        Vector<U32> const &src) {
    U32 ox{tx * (tile + 2u * pad)};
    U32 oy{ty * (tile + 2u * pad)};
    U32 sx{ox + pad};
    U32 sy{oy + pad};

    for (U32 y{0}; y < tile; ++y)
        for (U32 x{0}; x < tile; ++x)
            dst[(sy + y) * atlasW + (sx + x)] = src[y * tile + x];

    for (U32 x{ox}; x < ox + (2u * pad + tile); ++x) {
        U32 cx{std::clamp<U32>(x, sx, sx + tile - 1u)};
        for (U32 y{0}; y < pad; ++y) {
            dst[(sy - 1u - y) * atlasW + x] = dst[sy * atlasW + cx];
            dst[(sy + tile + y) * atlasW + x] = dst[(sy + tile - 1u) * atlasW + cx];
        }
    }

    for (U32 y{oy}; y < oy + (2u * pad + tile); ++y) {
        U32 cy{std::clamp<U32>(y, sy, sy + tile - 1u)};
        for (U32 x{0}; x < pad; ++x) {
            dst[y * atlasW + (sx - 1u - x)] = dst[cy * atlasW + sx];
            dst[y * atlasW + (sx + tile + x)] = dst[cy * atlasW + (sx + tile - 1u)];
        }
    }
}

export class VoxelRendererSystem : public System<VoxelRendererSystem> {
private:
    IGraphicsContext *m_Gfx{nullptr};
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

    void SetGraphicsContext(IGraphicsContext *gfx) {
        m_Gfx = gfx;
    }

    U32 CreatePipeline() const {
        ShaderCode vs{};
        vs.source = "voxel\\Voxel.hlsl";
        vs.entryPoint = "VSMain";
        vs.stage = ShaderStage::Vertex;
        ShaderCode ps{};
        ps.source = "voxel\\Voxel.hlsl";
        ps.entryPoint = "PSMain";
        ps.stage = ShaderStage::Pixel;
        GraphicsPipelineCreateInfo pi{};
        pi.shaders = {vs, ps};
        pi.vertexAttributes = {{"POSITION", 0}, {"NORMAL", 12}, {"TEXCOORD", 24}, {"COLOR", 32}};
        pi.vertexStride = sizeof(Vertex);
        pi.topology = PrimitiveTopology::TriangleList;
        pi.depthTest = true;
        pi.depthWrite = true;
        return m_Gfx->CreateGraphicsPipeline(pi);
    }

    void Run(World *world, F32) override {
        assert(m_Gfx != nullptr, "GraphicsContext must be set");

        auto *rr{world->GetStorage<VoxelRenderResources>()};
        if (!rr || rr->Size() == 0) {
            auto e{world->CreateEntity()};
            VoxelRenderResources r{};
            r.pipeline = CreatePipeline();
            world->AddComponent(e, r);
            rr = world->GetStorage<VoxelRenderResources>();
        }
        VoxelRenderResources const *res{};
        for (auto [h, r]: *rr) { res = &r; break; }
        assert(res != nullptr && res->pipeline != INVALID_INDEX, "Invalid render resources");

        if (m_CameraCB == INVALID_INDEX) m_CameraCB = m_Gfx->CreateConstantBuffer(sizeof(CameraConstants));
        if (m_ObjectCB == INVALID_INDEX) m_ObjectCB = m_Gfx->CreateConstantBuffer(sizeof(ObjectConstants));
        if (m_AtlasCB == INVALID_INDEX) m_AtlasCB = m_Gfx->CreateConstantBuffer(sizeof(AtlasConstants));

        if (m_AtlasTex == INVALID_INDEX) {
            const U32 pad{2};
            Vector<std::string> paths{
                "assets/dirt.png", "assets/grass_side.png", "assets/grass_top.png", "assets/stone.png",
                "assets/log_oak.png", "assets/log_oak_top.png", "assets/oak_leaves.png", "assets/sand.png",
                "assets/water.png"
            };
            Vector<Vector<U32>> imgs{};
            imgs.resize(paths.size());
            Vector<U32> w{};
            w.resize(paths.size());
            Vector<U32> h{};
            h.resize(paths.size());
            for (USize i{}; i < paths.size(); ++i) {
                bool ok{LoadImageRGBA(std::filesystem::path{paths[i]}, imgs[i], w[i], h[i])};
                assert(ok, "Failed to load atlas tile");
                assert(w[i] == h[i], "Tile must be square");
            }
            assert(std::ranges::all_of(w, [&](U32 x) { return x == w[0]; }), "Mismatched tile sizes");

            U32 tile{w[0]};
            U32 tilesX{static_cast<U32>(imgs.size())};
            U32 tilesY{1};
            U32 atlasW{tilesX * (tile + 2u * pad)};
            U32 atlasH{tilesY * (tile + 2u * pad)};
            Vector<U32> pixels{};
            pixels.resize(static_cast<USize>(atlasW * atlasH), 0u);

            for (U32 i{}; i < tilesX; ++i) {
                BlitWithPad(pixels, atlasW, atlasH, i, 0, tile, pad, imgs[i]);
            }

            m_AtlasTex = m_Gfx->CreateTexture2D(pixels.data(), atlasW, atlasH, 1);

            AtlasConstants ac{};
            ac.tilesX = tilesX;
            ac.tileSize = tile;
            ac.pad = pad;
            ac._pad0 = 0;
            ac.atlasW = atlasW;
            ac.atlasH = atlasH;
            m_Gfx->UpdateConstantBuffer(m_AtlasCB, &ac, sizeof(ac));

            VoxelAtlasInfo ai{};
            ai.texture = m_AtlasTex;
            ai.tilesX = tilesX;
            ai.tileSize = tile;
            ai.pad = pad;
            ai.atlasW = atlasW;
            ai.atlasH = atlasH;
            auto* storeAI{world->GetStorage<VoxelAtlasInfo>()};
            if (!storeAI || storeAI->Size() == 0) {
                auto e{world->CreateEntity()};
                world->AddComponent(e, ai);
            } else {
                for (auto [h, c] : *storeAI) { world->AddOrReplaceComponent(h, ai); break; }
            }
        }

        CameraConstants cam{};
        if (auto h{CameraManager::GetPrimaryCamera()}; h.valid()) {
            if (auto *c{world->GetComponent<Camera>(h)}) {
                cam.view = c->view;
                cam.projection = c->projection;
                cam.viewProjection = c->viewProjection;
            }
            if (auto *t{world->GetComponent<Transform>(h)}) {
                cam.cameraPosition = t->position;
            }
        }
        m_Gfx->UpdateConstantBuffer(m_CameraCB, &cam, sizeof(cam));

        Math::Frustum fr{};
        fr.SetFromMatrix(cam.viewProjection);

        auto* wcfgStore{world->GetStorage<VoxelWorldConfig>()};
        assert(wcfgStore && wcfgStore->Size() > 0, "Missing VoxelWorldConfig");
        VoxelWorldConfig const* wcfg{};
        for (auto [h, c] : *wcfgStore) { wcfg = &c; break; }
        const F32 sx{wcfg->blockSize * static_cast<F32>(VoxelChunk::SizeX)};
        const F32 sy{wcfg->blockSize * static_cast<F32>(VoxelChunk::SizeY)};
        const F32 sz{wcfg->blockSize * static_cast<F32>(VoxelChunk::SizeZ)};

        ObjectConstants obj{};
        obj.world = Math::Mat4::Identity;
        m_Gfx->UpdateConstantBuffer(m_ObjectCB, &obj, sizeof(obj));

        auto *storage{world->GetStorage<VoxelMesh>()};
        if (!storage) return;

        auto* sStore{world->GetStorage<VoxelCullingStats>()};
        if (!sStore || sStore->Size() == 0) {
            auto e{world->CreateEntity()};
            world->AddComponent(e, VoxelCullingStats{});
            sStore = world->GetStorage<VoxelCullingStats>();
        }
        VoxelCullingStats stats{};

        m_Gfx->SetPipeline(res->pipeline);
        m_Gfx->SetConstantBuffer(m_CameraCB, 0);
        m_Gfx->SetConstantBuffer(m_ObjectCB, 1);
        m_Gfx->SetTexture(m_AtlasTex, 0);
        m_Gfx->SetConstantBuffer(m_AtlasCB, 2);

        for (auto [handle, mesh]: *storage) {
            if (mesh.vertexBuffer == INVALID_INDEX || mesh.vertexCount == 0) continue;

            auto* chunk{world->GetComponent<VoxelChunk>(handle)};
            if (!chunk) continue;

            stats.tested++;

            Math::Bounds b{chunk->origin, chunk->origin + Math::Vec3{sx, sy, sz}};
            if (!fr.Intersects(b)) { stats.culled++; continue; }

            stats.visible++;
            m_Gfx->SetVertexBuffer(mesh.vertexBuffer);
            if (mesh.indexBuffer != INVALID_INDEX && mesh.indexCount > 0) {
                m_Gfx->SetIndexBuffer(mesh.indexBuffer);
                m_Gfx->DrawIndexed(mesh.indexCount);
                stats.drawCalls++;
                stats.drawnIndices += static_cast<U64>(mesh.indexCount);
            } else {
                m_Gfx->Draw(mesh.vertexCount);
                stats.drawCalls++;
                stats.drawnVerts += static_cast<U64>(mesh.vertexCount);
            }
        }

        for (auto [h, s] : *sStore) { world->AddOrReplaceComponent(h, stats); break; }
    }
};
