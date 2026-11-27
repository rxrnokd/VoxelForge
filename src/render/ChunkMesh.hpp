#pragma once

#include "world/Block.hpp"

#include <cstdint>
#include <vector>

namespace voxelforge {

// Compact 8 byte vertex. Positions are fixed point (1/16 block) relative to the chunk
// origin, which keeps them exact and lets a whole chunk column (16 x 256 x 16) fit into
// 16-bit integers. Texture coordinates are not stored at all: the fragment shader derives
// them from the position and face, which also makes greedy quads tile correctly.
struct ChunkVertex {
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    std::uint16_t z = 0;
    // bits 0-2 face, 3-4 ambient occlusion (0..3), 5-12 atlas tile, 13-15 sky light (0..4)
    std::uint16_t data = 0;

    static constexpr std::uint16_t kSubUnits = 16; // fixed point scale
    static constexpr int kMaxSky = 4;

    static constexpr std::uint16_t pack(Face face, int ao, Tile tile, int sky = kMaxSky) {
        return static_cast<std::uint16_t>(static_cast<unsigned>(face) | (static_cast<unsigned>(ao) << 3) |
                                          (static_cast<unsigned>(tile) << 5) | (static_cast<unsigned>(sky) << 13));
    }
    constexpr Face face() const { return static_cast<Face>(data & 7u); }
    constexpr int ao() const { return (data >> 3) & 3; }
    constexpr Tile tile() const { return static_cast<Tile>((data >> 5) & 255u); }
    constexpr int sky() const { return (data >> 13) & 7; }
};
static_assert(sizeof(ChunkVertex) == 8, "ChunkVertex must stay 8 bytes");

// CPU side mesh of one chunk: opaque/cutout geometry plus the translucent water layer.
// Triangle lists (6 vertices per quad) avoid the 16-bit index limit of raylib's API.
struct ChunkMesh {
    std::vector<ChunkVertex> opaque;
    std::vector<ChunkVertex> water;
    int minY = 0; // vertical extent of the geometry, used for tighter frustum culling
    int maxY = 0;
    int quadCount = 0;

    void clear() {
        opaque.clear();
        water.clear();
        minY = 0;
        maxY = 0;
        quadCount = 0;
    }
    bool empty() const { return opaque.empty() && water.empty(); }
    std::size_t vertexCount() const { return opaque.size() + water.size(); }
};

} // namespace voxelforge
