#pragma once

#include "world/Chunk.hpp"
#include "world/Noise.hpp"

#include <cstdint>

namespace voxelforge {

// Generates chunk contents as a pure function of (seed, chunk position). The generator
// is immutable after construction and safe to share between worker threads.
class TerrainGenerator {
public:
    explicit TerrainGenerator(std::uint64_t seed);

    std::uint64_t seed() const { return m_seed; }

    void generate(ChunkPos pos, Chunk& out) const;

    // Y of the topmost terrain block (before caves/water/trees) of a world column.
    int surfaceHeight(int wx, int wz) const;

private:
    std::uint64_t m_seed;
    PerlinNoise m_continental;
    PerlinNoise m_detail;
};

} // namespace voxelforge
