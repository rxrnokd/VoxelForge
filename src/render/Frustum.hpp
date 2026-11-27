#pragma once

#include "core/Math.hpp"

#include <array>

namespace voxelforge {

// View frustum as six inward facing planes, extracted directly from a view-projection
// matrix (Gribb & Hartmann). Used to skip chunks that cannot be on screen.
class Frustum {
public:
    explicit Frustum(const Mat4& viewProjection);

    // Conservative test: false only when the box is completely outside one plane.
    bool intersectsBox(const Vec3& min, const Vec3& max) const;
    bool containsPoint(const Vec3& p) const;

private:
    struct Plane {
        Vec3 normal;
        float d = 0.0f;
        float distance(const Vec3& p) const { return dot(normal, p) + d; }
    };
    std::array<Plane, 6> m_planes; // left, right, bottom, top, near, far
};

} // namespace voxelforge
