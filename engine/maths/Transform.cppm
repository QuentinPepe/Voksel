export module Math.Transform;

import Core.Types;
import Math.Core;
import Math.Vector;
import Math.Matrix;
import Math.Quaternion;
import Components.Transform;
import std;

export namespace Math {
    // Common math functions
    struct Bounds {
        Vec3 min{std::numeric_limits<F32>::max()};
        Vec3 max{std::numeric_limits<F32>::lowest()};

        constexpr Bounds() = default;
        constexpr Bounds(const Vec3& min_, const Vec3& max_) : min{min_}, max{max_} {}

        [[nodiscard]] Vec3 Center() const { return (min + max) * 0.5f; }
        [[nodiscard]] Vec3 Size() const { return max - min; }
        [[nodiscard]] Vec3 Extents() const { return Size() * 0.5f; }

        [[nodiscard]] bool Contains(const Vec3& point) const {
            return point.x >= min.x && point.x <= max.x &&
                   point.y >= min.y && point.y <= max.y &&
                   point.z >= min.z && point.z <= max.z;
        }

        void Expand(const Vec3& point) {
            min.x = std::min(min.x, point.x);
            min.y = std::min(min.y, point.y);
            min.z = std::min(min.z, point.z);
            max.x = std::max(max.x, point.x);
            max.y = std::max(max.y, point.y);
            max.z = std::max(max.z, point.z);
        }

        void Expand(const Bounds& other) {
            Expand(other.min);
            Expand(other.max);
        }

        [[nodiscard]] Bounds Transformed(const Transform& transform) const {
            Bounds result;
            Vec3 corners[8] = {
                {min.x, min.y, min.z}, {max.x, min.y, min.z},
                {min.x, max.y, min.z}, {max.x, max.y, min.z},
                {min.x, min.y, max.z}, {max.x, min.y, max.z},
                {min.x, max.y, max.z}, {max.x, max.y, max.z}
            };

            for (const auto& corner : corners) {
                result.Expand(transform.TransformPoint(corner));
            }

            return result;
        }
    };

    struct Ray {
        Vec3 origin;
        Vec3 direction;

        constexpr Ray() : direction{Vec3::Forward} {}
        Ray(const Vec3& o, const Vec3& d) : origin{o}, direction{d.Normalized()} {}

        [[nodiscard]] Vec3 GetPoint(F32 distance) const {
            return origin + direction * distance;
        }

        [[nodiscard]] bool IntersectsBounds(const Bounds& bounds, F32& tMin, F32& tMax) const {
            Vec3 invDir{1.0f / direction.x, 1.0f / direction.y, 1.0f / direction.z};

            Vec3 t1 = (bounds.min - origin) * invDir;
            Vec3 t2 = (bounds.max - origin) * invDir;

            Vec3 tMinVec = Vec3{std::min(t1.x, t2.x), std::min(t1.y, t2.y), std::min(t1.z, t2.z)};
            Vec3 tMaxVec = Vec3{std::max(t1.x, t2.x), std::max(t1.y, t2.y), std::max(t1.z, t2.z)};

            tMin = std::max({tMinVec.x, tMinVec.y, tMinVec.z});
            tMax = std::min({tMaxVec.x, tMaxVec.y, tMaxVec.z});

            return tMax >= tMin && tMax >= 0.0f;
        }

        [[nodiscard]] bool IntersectsSphere(const Vec3& center, F32 radius, F32& t) const {
            Vec3 oc = origin - center;
            F32 a = direction.LengthSquared();
            F32 b = 2.0f * oc.Dot(direction);
            F32 c = oc.LengthSquared() - radius * radius;
            F32 discriminant = b * b - 4.0f * a * c;

            if (discriminant < 0.0f) return false;

            F32 sqrtDisc = std::sqrt(discriminant);
            F32 t0 = (-b - sqrtDisc) / (2.0f * a);
            F32 t1 = (-b + sqrtDisc) / (2.0f * a);

            if (t0 > t1) std::swap(t0, t1);

            if (t0 < 0.0f) {
                t0 = t1;
                if (t0 < 0.0f) return false;
            }

            t = t0;
            return true;
        }
    };

    struct Plane {
        Vec3 normal;
        F32 distance;

        constexpr Plane() : normal{Vec3::Up}, distance{0.0f} {}
        Plane(const Vec3& n, F32 d) : normal{n.Normalized()}, distance{d} {}
        Plane(const Vec3& n, const Vec3& point) : normal{n.Normalized()}, distance{normal.Dot(point)} {}

        [[nodiscard]] F32 DistanceToPoint(const Vec3& point) const {
            return normal.Dot(point) - distance;
        }

        [[nodiscard]] Vec3 ClosestPoint(const Vec3& point) const {
            return point - normal * DistanceToPoint(point);
        }

        [[nodiscard]] bool IsAbove(const Vec3& point) const {
            return DistanceToPoint(point) > 0.0f;
        }
    };

    // Frustum for culling
    struct Frustum {
        Plane planes[6]; // Near, Far, Left, Right, Top, Bottom

        void SetFromMatrix(const Mat4& viewProj) {
            // Extract frustum planes from view-projection matrix
            auto m = viewProj.Transposed();

            // Left plane
            planes[2] = Plane{
                Vec3{m.m[3][0] + m.m[0][0], m.m[3][1] + m.m[0][1], m.m[3][2] + m.m[0][2]},
                m.m[3][3] + m.m[0][3]
            };

            // Right plane
            planes[3] = Plane{
                Vec3{m.m[3][0] - m.m[0][0], m.m[3][1] - m.m[0][1], m.m[3][2] - m.m[0][2]},
                m.m[3][3] - m.m[0][3]
            };

            // Bottom plane
            planes[5] = Plane{
                Vec3{m.m[3][0] + m.m[1][0], m.m[3][1] + m.m[1][1], m.m[3][2] + m.m[1][2]},
                m.m[3][3] + m.m[1][3]
            };

            // Top plane
            planes[4] = Plane{
                Vec3{m.m[3][0] - m.m[1][0], m.m[3][1] - m.m[1][1], m.m[3][2] - m.m[1][2]},
                m.m[3][3] - m.m[1][3]
            };

            // Near plane
            planes[0] = Plane{
                Vec3{m.m[3][0] + m.m[2][0], m.m[3][1] + m.m[2][1], m.m[3][2] + m.m[2][2]},
                m.m[3][3] + m.m[2][3]
            };

            // Far plane
            planes[1] = Plane{
                Vec3{m.m[3][0] - m.m[2][0], m.m[3][1] - m.m[2][1], m.m[3][2] - m.m[2][2]},
                m.m[3][3] - m.m[2][3]
            };

            // Normalize all planes
            for (auto& plane : planes) {
                F32 len = plane.normal.Length();
                plane.normal /= len;
                plane.distance /= len;
            }
        }

        [[nodiscard]] bool Contains(const Vec3& point) const {
            for (const auto& plane : planes) {
                if (plane.DistanceToPoint(point) < 0.0f) return false;
            }
            return true;
        }

        [[nodiscard]] bool Intersects(const Bounds& bounds) const {
            Vec3 center = bounds.Center();
            Vec3 extents = bounds.Extents();

            for (const auto& plane : planes) {
                F32 r = extents.x * std::abs(plane.normal.x) +
                       extents.y * std::abs(plane.normal.y) +
                       extents.z * std::abs(plane.normal.z);

                if (plane.DistanceToPoint(center) < -r) return false;
            }
            return true;
        }
    };
}