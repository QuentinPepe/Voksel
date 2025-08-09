export module Systems.VoxelRaycast;

import ECS.World;
import Components.ComponentRegistry;
import Components.Voxel;
import Math.Core;
import Math.Vector;
import Core.Assert;
import Core.Types;
import std;

export struct VoxelRayHit {
    bool hit{false};
    S32 gx{0}, gy{0}, gz{0};
    S32 pgx{0}, pgy{0}, pgz{0};
    Math::Vec3 normal{};
    F32 t{0.0f};
};

namespace detail {
    inline S32 floordiv(S32 a, S32 b) {
        return static_cast<S32>(std::floor(static_cast<F32>(a) / static_cast<F32>(b)));
    }

    inline bool FetchChunk(World* w, S32 cx, S32 cy, S32 cz, VoxelChunk*& out) {
        out = nullptr;
        if (auto* s = w->GetStorage<VoxelChunk>()) {
            for (auto [h,c] : *s) {
                if (static_cast<S32>(c.cx)==cx && static_cast<S32>(c.cy)==cy && static_cast<S32>(c.cz)==cz) {
                    out = const_cast<VoxelChunk*>(&c);
                    return true;
                }
            }
        }
        return false;
    }

    inline bool GetVoxel(World* w, S32 gx, S32 gy, S32 gz, Voxel& out) {
        constexpr S32 NX{static_cast<S32>(VoxelChunk::SizeX)};
        constexpr S32 NY{static_cast<S32>(VoxelChunk::SizeY)};
        constexpr S32 NZ{static_cast<S32>(VoxelChunk::SizeZ)};
        S32 cx{floordiv(gx, NX)}, cy{floordiv(gy, NY)}, cz{floordiv(gz, NZ)};
        S32 lx{gx - cx*NX}, ly{gy - cy*NY}, lz{gz - cz*NZ};
        VoxelChunk* ch{};
        if (!FetchChunk(w, cx, cy, cz, ch)) return false;
        if (ch->blocks.empty()) return false;
        out = ch->blocks[VoxelIndex(static_cast<U32>(lx), static_cast<U32>(ly), static_cast<U32>(lz))];
        return true;
    }
}

export VoxelRayHit RaycastVoxelDDA(World* world, Math::Vec3 origin, Math::Vec3 dir, F32 maxDist) {
    VoxelRayHit hit{};
    auto* cfgStore{world->GetStorage<VoxelWorldConfig>()};
    assert(cfgStore && cfgStore->Size()>0, "Missing VoxelWorldConfig");
    VoxelWorldConfig const* cfg{};
    for (auto [h,c] : *cfgStore) { cfg = &c; break; }
    const F32 bs{cfg->blockSize};

    Math::Vec3 rd{dir.Normalized()};
    const F32 eps{1e-6f};
    const F32 inf{1e30f};

    auto wx = static_cast<F32>(std::floor(origin.x / bs));
    auto wy = static_cast<F32>(std::floor(origin.y / bs));
    auto wz = static_cast<F32>(std::floor(origin.z / bs));
    S32 gx{static_cast<S32>(wx)}, gy{static_cast<S32>(wy)}, gz{static_cast<S32>(wz)};

    S32 stepx{rd.x>0.0f?1:-1}, stepy{rd.y>0.0f?1:-1}, stepz{rd.z>0.0f?1:-1};

    auto nextBoundary = [&](F32 o, F32 d, S32 g, S32 s)->F32 {
        F32 nb = (static_cast<F32>(g + (s>0?1:0)) * bs - o) / (std::abs(d)<eps? (s>0?+eps:-eps) : d);
        return nb < 0.0f ? 0.0f : nb;
    };
    auto delta = [&](F32 d)->F32 { return std::abs(d)<eps ? inf : (bs / std::abs(d)); };

    F32 tMaxX{nextBoundary(origin.x, rd.x, gx, stepx)};
    F32 tMaxY{nextBoundary(origin.y, rd.y, gy, stepy)};
    F32 tMaxZ{nextBoundary(origin.z, rd.z, gz, stepz)};
    F32 tDeltaX{delta(rd.x)}, tDeltaY{delta(rd.y)}, tDeltaZ{delta(rd.z)};

    S32 px{gx}, py{gy}, pz{gz};
    for (;;) {
        Voxel v{};
        if (detail::GetVoxel(world, gx, gy, gz, v) && v != Voxel::Air) {
            hit.hit = true;
            hit.gx = gx; hit.gy = gy; hit.gz = gz;
            hit.pgx = px; hit.pgy = py; hit.pgz = pz;
            hit.t = std::min({tMaxX, tMaxY, tMaxZ});
            if (tMaxX <= tMaxY && tMaxX <= tMaxZ) hit.normal = Math::Vec3{static_cast<F32>(-stepx),0.0f,0.0f};
            else if (tMaxY <= tMaxZ) hit.normal = Math::Vec3{0.0f,static_cast<F32>(-stepy),0.0f};
            else hit.normal = Math::Vec3{0.0f,0.0f,static_cast<F32>(-stepz)};
            return hit;
        }

        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                px=gx; py=gy; pz=gz; gx += stepx; hit.t = tMaxX; tMaxX += tDeltaX;
            } else {
                px=gx; py=gy; pz=gz; gz += stepz; hit.t = tMaxZ; tMaxZ += tDeltaZ;
            }
        } else {
            if (tMaxY < tMaxZ) {
                px=gx; py=gy; pz=gz; gy += stepy; hit.t = tMaxY; tMaxY += tDeltaY;
            } else {
                px=gx; py=gy; pz=gz; gz += stepz; hit.t = tMaxZ; tMaxZ += tDeltaZ;
            }
        }

        if (hit.t > maxDist) return VoxelRayHit{};
    }
}
