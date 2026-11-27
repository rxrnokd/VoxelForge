#pragma once

#include "core/Math.hpp"
#include "world/Block.hpp"

namespace voxelforge {

// Axis aligned bounding box in world space.
struct AABB {
    Vec3 min;
    Vec3 max;

    static constexpr AABB fromBlock(int x, int y, int z) {
        return {{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)},
                {static_cast<float>(x + 1), static_cast<float>(y + 1), static_cast<float>(z + 1)}};
    }
    // Box centred horizontally on `feet` with its bottom at feet.y.
    static constexpr AABB fromFeet(const Vec3& feet, float width, float height) {
        const float h = width * 0.5f;
        return {{feet.x - h, feet.y, feet.z - h}, {feet.x + h, feet.y + height, feet.z + h}};
    }

    constexpr AABB translated(const Vec3& d) const { return {min + d, max + d}; }

    // Strict overlap: boxes that merely touch do not intersect.
    constexpr bool intersects(const AABB& o) const {
        return min.x < o.max.x && max.x > o.min.x && min.y < o.max.y && max.y > o.min.y && min.z < o.max.z &&
               max.z > o.min.z;
    }
};

// Read-only access to blocks, implemented by World (and by simple grids in tests).
class VoxelQuery {
public:
    virtual ~VoxelQuery() = default;
    virtual BlockType blockAt(int x, int y, int z) const = 0;
};

} // namespace voxelforge
