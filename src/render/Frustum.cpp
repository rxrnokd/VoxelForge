#include "render/Frustum.hpp"

namespace voxelforge {

Frustum::Frustum(const Mat4& m) {
    // Clip space position c = M * v. A point is inside when -w <= x, y, z <= w, so each
    // plane is the 4th row of M plus or minus one of the first three rows.
    auto row = [&m](int r) { return std::array<float, 4>{m.at(r, 0), m.at(r, 1), m.at(r, 2), m.at(r, 3)}; };
    const auto r0 = row(0);
    const auto r1 = row(1);
    const auto r2 = row(2);
    const auto r3 = row(3);
    const std::array<float, 4>* rows[3] = {&r0, &r1, &r2};
    for (int i = 0; i < 3; ++i) {
        for (int s = 0; s < 2; ++s) {
            const float sign = s == 0 ? 1.0f : -1.0f;
            const auto& r = *rows[i];
            Plane p;
            p.normal = {r3[0] + sign * r[0], r3[1] + sign * r[1], r3[2] + sign * r[2]};
            p.d = r3[3] + sign * r[3];
            const float len = length(p.normal);
            if (len > 0.0f) {
                p.normal = p.normal / len;
                p.d /= len;
            }
            m_planes[static_cast<std::size_t>(i * 2 + s)] = p;
        }
    }
}

bool Frustum::intersectsBox(const Vec3& min, const Vec3& max) const {
    for (const Plane& p : m_planes) {
        // The box corner furthest along the plane normal (the "positive vertex").
        const Vec3 positive{p.normal.x >= 0.0f ? max.x : min.x, p.normal.y >= 0.0f ? max.y : min.y,
                            p.normal.z >= 0.0f ? max.z : min.z};
        if (p.distance(positive) < 0.0f) {
            return false;
        }
    }
    return true;
}

bool Frustum::containsPoint(const Vec3& point) const {
    for (const Plane& p : m_planes) {
        if (p.distance(point) < 0.0f) {
            return false;
        }
    }
    return true;
}

} // namespace voxelforge
