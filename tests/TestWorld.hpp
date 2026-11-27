#pragma once

#include "physics/AABB.hpp"

#include <map>
#include <tuple>

namespace voxelforge::test {

// Simple voxel world for physics tests: solid ground below `groundHeight` plus any
// explicitly placed blocks.
class TestWorld : public VoxelQuery {
public:
    explicit TestWorld(int groundHeight = 64) : m_ground(groundHeight) {}

    BlockType blockAt(int x, int y, int z) const override {
        const auto it = m_blocks.find({x, y, z});
        if (it != m_blocks.end()) {
            return it->second;
        }
        return y < m_ground ? BlockType::Stone : BlockType::Air;
    }
    void set(int x, int y, int z, BlockType type) { m_blocks[{x, y, z}] = type; }

private:
    int m_ground;
    std::map<std::tuple<int, int, int>, BlockType> m_blocks;
};

} // namespace voxelforge::test
