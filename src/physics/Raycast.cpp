#include "physics/Raycast.hpp"

#include "world/Chunk.hpp"

#include <cmath>
#include <limits>

namespace voxelforge {

std::optional<RaycastHit> raycastBlocks(const VoxelQuery& world, const Vec3& origin, const Vec3& direction,
                                        float maxDistance) {
    const Vec3 dir = normalize(direction);
    if (dir == Vec3{}) {
        return std::nullopt;
    }
    constexpr float kInfinity = std::numeric_limits<float>::infinity();

    IVec3 cell{fastFloor(origin.x), fastFloor(origin.y), fastFloor(origin.z)};
    IVec3 step;
    Vec3 tMax;   // ray distance at which the next boundary on each axis is crossed
    Vec3 tDelta; // ray distance between two boundaries on each axis
    for (int a = 0; a < 3; ++a) {
        if (dir[a] > 0.0f) {
            step[a] = 1;
            tDelta[a] = 1.0f / dir[a];
            tMax[a] = (static_cast<float>(cell[a] + 1) - origin[a]) * tDelta[a];
        } else if (dir[a] < 0.0f) {
            step[a] = -1;
            tDelta[a] = -1.0f / dir[a];
            tMax[a] = (origin[a] - static_cast<float>(cell[a])) * tDelta[a];
        } else {
            step[a] = 0;
            tDelta[a] = kInfinity;
            tMax[a] = kInfinity;
        }
    }

    IVec3 normal;
    float distance = 0.0f;
    while (distance <= maxDistance) {
        if (cell.y >= 0 && cell.y < kChunkHeight) {
            const BlockType type = world.blockAt(cell.x, cell.y, cell.z);
            if (isSelectable(type)) {
                return RaycastHit{cell, normal, distance, type};
            }
        } else if ((cell.y < 0 && step.y <= 0) || (cell.y >= kChunkHeight && step.y >= 0)) {
            break; // left the world vertically and moving further away
        }

        // Advance to the nearest boundary.
        int axis = 0;
        if (tMax.y < tMax[axis]) axis = 1;
        if (tMax.z < tMax[axis]) axis = 2;
        distance = tMax[axis];
        cell[axis] += step[axis];
        tMax[axis] += tDelta[axis];
        normal = {};
        normal[axis] = -step[axis];
    }
    return std::nullopt;
}

} // namespace voxelforge
