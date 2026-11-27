#pragma once

#include "physics/AABB.hpp"

#include <optional>

namespace voxelforge {

struct RaycastHit {
    IVec3 block;     // the block that was hit
    IVec3 normal;    // outward normal of the face that was entered (zero if the ray starts inside)
    float distance;  // distance from the ray origin to the entry point
    BlockType type;

    IVec3 placementTarget() const { return block + normal; }
};

// Voxel traversal (Amanatides & Woo DDA): steps from cell boundary to cell boundary
// along the ray, visiting every block it passes through exactly once, until a
// selectable block is found or `maxDistance` is exceeded.
std::optional<RaycastHit> raycastBlocks(const VoxelQuery& world, const Vec3& origin, const Vec3& direction,
                                        float maxDistance);

} // namespace voxelforge
