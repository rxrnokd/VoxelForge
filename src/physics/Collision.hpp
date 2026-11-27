#pragma once

#include "physics/AABB.hpp"

namespace voxelforge {

struct CollisionResult {
    Vec3 applied;        // movement actually performed
    bool hitX = false;
    bool hitY = false;
    bool hitZ = false;
    bool landed = false; // blocked while moving down: standing on something
};

// Moves `box` by `delta` through the voxel world, one axis at a time (Y, then X, then Z),
// clipping the motion against every solid block in the swept region. Because the whole
// swept volume is examined, fast movement can never tunnel through blocks.
CollisionResult moveAndCollide(const VoxelQuery& world, AABB& box, const Vec3& delta);

// True when any solid block overlaps the box.
bool overlapsSolid(const VoxelQuery& world, const AABB& box);

} // namespace voxelforge
