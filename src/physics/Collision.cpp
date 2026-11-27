#include "physics/Collision.hpp"

#include <algorithm>
#include <cmath>

namespace voxelforge {
namespace {

constexpr float kEpsilon = 1e-4f;

// Block coordinate range strictly covered by [lo, hi] (touching faces excluded).
inline int firstBlock(float lo) { return static_cast<int>(std::floor(lo + kEpsilon)); }
inline int lastBlock(float hi) { return static_cast<int>(std::floor(hi - kEpsilon)); }

float sweepAxis(const VoxelQuery& world, const AABB& box, int axis, float delta) {
    if (delta == 0.0f) {
        return 0.0f;
    }
    const float wanted = delta;
    Vec3 lo = box.min;
    Vec3 hi = box.max;
    if (delta > 0.0f) {
        hi[axis] += delta;
    } else {
        lo[axis] += delta;
    }

    int from[3];
    int to[3];
    for (int a = 0; a < 3; ++a) {
        from[a] = firstBlock(lo[a]);
        to[a] = lastBlock(hi[a]);
    }
    // Along the movement axis include the block we are touching right now.
    if (delta > 0.0f) {
        from[axis] = static_cast<int>(std::floor(box.max[axis] - kEpsilon));
    } else {
        to[axis] = static_cast<int>(std::floor(box.min[axis] + kEpsilon));
    }

    for (int y = from[1]; y <= to[1]; ++y) {
        for (int z = from[2]; z <= to[2]; ++z) {
            for (int x = from[0]; x <= to[0]; ++x) {
                if (!isSolid(world.blockAt(x, y, z))) {
                    continue;
                }
                const AABB block = AABB::fromBlock(x, y, z);
                if (wanted > 0.0f && box.max[axis] <= block.min[axis] + kEpsilon) {
                    delta = std::min(delta, block.min[axis] - box.max[axis]);
                } else if (wanted < 0.0f && box.min[axis] >= block.max[axis] - kEpsilon) {
                    delta = std::max(delta, block.max[axis] - box.min[axis]);
                }
            }
        }
    }
    // Never move backwards because of float noise when already touching.
    delta = wanted > 0.0f ? std::max(delta, 0.0f) : std::min(delta, 0.0f);
    return std::fabs(delta) < 1e-6f ? 0.0f : delta;
}

} // namespace

CollisionResult moveAndCollide(const VoxelQuery& world, AABB& box, const Vec3& delta) {
    CollisionResult result;
    static constexpr int kOrder[3] = {1, 0, 2}; // vertical first so walking off edges feels right
    for (const int axis : kOrder) {
        const float wanted = delta[axis];
        const float moved = sweepAxis(world, box, axis, wanted);
        if (moved != 0.0f) {
            Vec3 step;
            step[axis] = moved;
            box = box.translated(step);
        }
        result.applied[axis] = moved;
        const bool blocked = std::fabs(moved - wanted) > 1e-6f;
        if (axis == 0) result.hitX = blocked;
        if (axis == 1) {
            result.hitY = blocked;
            result.landed = blocked && wanted < 0.0f;
        }
        if (axis == 2) result.hitZ = blocked;
    }
    return result;
}

bool overlapsSolid(const VoxelQuery& world, const AABB& box) {
    for (int y = firstBlock(box.min.y); y <= lastBlock(box.max.y); ++y) {
        for (int z = firstBlock(box.min.z); z <= lastBlock(box.max.z); ++z) {
            for (int x = firstBlock(box.min.x); x <= lastBlock(box.max.x); ++x) {
                if (isSolid(world.blockAt(x, y, z))) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace voxelforge
