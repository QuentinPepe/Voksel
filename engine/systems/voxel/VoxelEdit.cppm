export module Systems.VoxelEdit;

import ECS.SystemScheduler;
import ECS.World;
import ECS.Component;
import Components.Voxel;
import Components.Transform;
import Components.Camera;
import Components.VoxelSelection;
import Systems.CameraManager;
import Systems.VoxelRaycast;
import Input.Core;
import Input.Manager;
import Input.Bindings;
import Math.Vector;
import Core.Types;
import Core.Assert;
import std;

namespace detail {
    inline S32 floordiv(S32 a, S32 b) {
        return static_cast<S32>(std::floor(static_cast<F32>(a) / static_cast<F32>(b)));
    }

    inline void MarkDirtyNeighbors(World* w, VoxelChunk* ch) {
        if (auto* store = w->GetStorage<VoxelChunk>()) {
            for (auto [hh, cc] : *store) {
                if (cc.cx + 1 == ch->cx && cc.cy == ch->cy && cc.cz == ch->cz) const_cast<VoxelChunk&>(cc).dirty = true;
                if (cc.cx == ch->cx + 1 && cc.cy == ch->cy && cc.cz == ch->cz) const_cast<VoxelChunk&>(cc).dirty = true;
                if (cc.cy + 1 == ch->cy && cc.cx == ch->cx && cc.cz == ch->cz) const_cast<VoxelChunk&>(cc).dirty = true;
                if (cc.cy == ch->cy + 1 && cc.cx == ch->cx && cc.cz == ch->cz) const_cast<VoxelChunk&>(cc).dirty = true;
                if (cc.cz + 1 == ch->cz && cc.cx == ch->cx && cc.cy == ch->cy) const_cast<VoxelChunk&>(cc).dirty = true;
                if (cc.cz == ch->cz + 1 && cc.cx == ch->cx && cc.cy == ch->cy) const_cast<VoxelChunk&>(cc).dirty = true;
            }
        }
    }

    inline bool SetVoxel(World* w, S32 gx, S32 gy, S32 gz, Voxel v) {
        constexpr S32 NX{static_cast<S32>(VoxelChunk::SizeX)};
        constexpr S32 NY{static_cast<S32>(VoxelChunk::SizeY)};
        constexpr S32 NZ{static_cast<S32>(VoxelChunk::SizeZ)};
        S32 cx{floordiv(gx, NX)}, cy{floordiv(gy, NY)}, cz{floordiv(gz, NZ)};
        S32 lx{gx - cx*NX}, ly{gy - cy*NY}, lz{gz - cz*NZ};

        VoxelChunk* ch{};
        if (auto* s = w->GetStorage<VoxelChunk>()) {
            for (auto [h,c] : *s) {
                if (static_cast<S32>(c.cx)==cx && static_cast<S32>(c.cy)==cy && static_cast<S32>(c.cz)==cz) {
                    ch = const_cast<VoxelChunk*>(&c);
                    break;
                }
            }
        }
        if (!ch || ch->blocks.empty()) return false;

        ch->blocks[VoxelIndex(static_cast<U32>(lx), static_cast<U32>(ly), static_cast<U32>(lz))] = v;
        ch->dirty = true;
        MarkDirtyNeighbors(w, ch);
        return true;
    }
}

export class VoxelEditSystem : public System<VoxelEditSystem> {
private:
    InputManager* m_Input{nullptr};
    std::shared_ptr<InputContext> m_Ctx;
    F32 m_MaxDist{6.0f};

public:
    void Setup() {
        SetName("VoxelEdit");
        SetStage(SystemStage::Update);
        SetPriority(SystemPriority::High);
        RunBefore("VoxelMeshing");
        SetupInput();
    }

    void SetInputManager(InputManager* im) { m_Input = im; SetupInput(); }

    void Run(World* world, F32) override {
        if (!m_Input || !m_Ctx) return;

        Math::Vec3 camPos{};
        Math::Vec3 camDir{};
        if (auto h{CameraManager::GetPrimaryCamera()}; h.valid()) {
            if (auto* t = world->GetComponent<Transform>(h)) {
                camPos = t->position;
                camDir = t->Forward();
            }
        }

        auto hit = RaycastVoxelDDA(world, camPos, camDir, m_MaxDist);

        VoxelSelection sel{};
        if (hit.hit) {
            sel.valid = true;
            sel.gx = hit.gx; sel.gy = hit.gy; sel.gz = hit.gz;
            sel.pgx = hit.pgx; sel.pgy = hit.pgy; sel.pgz = hit.pgz;
            sel.normal = hit.normal;
        }

        {
            EntityHandle e{};
            auto* s = world->GetStorage<VoxelSelection>();
            if (!s || s->Size()==0) { e = world->CreateEntity(); world->AddComponent(e, sel); }
            else {
                for (auto [h,c] : *s) { e = h; break; }
                world->AddOrReplaceComponent(e, sel);
            }
        }

        m_Ctx->Update(*m_Input);
        bool doBreak = m_Ctx->GetAction("VoxelBreak")->JustPressed();
        bool doPlace = m_Ctx->GetAction("VoxelPlace")->JustPressed();

        if (!sel.valid) return;

        if (doBreak) { detail::SetVoxel(world, sel.gx, sel.gy, sel.gz, Voxel::Air); }
        if (doPlace) {
            Voxel place{Voxel::Dirt};
            if (auto* st = world->GetStorage<VoxelHotbarState>()) {
                for (auto [h, hb] : *st) { place = hb.selected; break; }
            }
            detail::SetVoxel(world, sel.pgx, sel.pgy, sel.pgz, place);
        }
    }

private:
    void SetupInput() {
        if (!m_Input) return;
        InputMapper mapper{};
        m_Ctx = mapper.CreateContext("VoxelEdit", 90);
        auto* aBreak = m_Ctx->AddAction("VoxelBreak");
        aBreak->AddBinding(InputBinding::MakeMouseButton(MouseButton::Left));
        auto* aPlace = m_Ctx->AddAction("VoxelPlace");
        aPlace->AddBinding(InputBinding::MakeMouseButton(MouseButton::Right));
        m_Ctx->Update(*m_Input);
    }
};
