#pragma once

#include "world/Biome.hpp"
#include "world/Chunk.hpp"
#include "world/Noise.hpp"

#include <cstdint>

namespace voxelforge {

// Everything the generator knows about one world column before carving caves.
struct ColumnInfo {
    int height = 0;                 // y of the topmost terrain block
    Biome biome = Biome::Plains;    // surface biome (dithered along borders)
    Biome dominant = Biome::Plains; // biome with the largest blend weight
    bool caveEntrance = false;      // caves may break through the surface here
};

// Generates chunk contents as a pure function of (seed, chunk position):
//   1. climate noise (temperature, humidity) -> blended biome weights
//   2. blended biome heights + continental noise (oceans) -> surface height
//   3. surface layers per biome, water up to sea level, bedrock floor
//   4. caves from interpolated 3D noise (spaghetti tunnels + caverns)
//   5. ore clusters by depth, then trees/cacti
// Every decision depends only on world coordinates and the seed, so neighbouring chunks
// agree on features that cross their border (trees) and generation order is irrelevant.
// The generator is immutable after construction and safe to share between threads.
class TerrainGenerator {
public:
    explicit TerrainGenerator(std::uint64_t seed);

    std::uint64_t seed() const { return m_seed; }

    void generate(ChunkPos pos, Chunk& out) const;

    ColumnInfo column(int wx, int wz) const;
    int surfaceHeight(int wx, int wz) const { return column(wx, wz).height; }
    Biome biomeAt(int wx, int wz) const { return column(wx, wz).dominant; }

    // Ore depth ranges (inclusive), exposed for tests and documentation.
    struct OreRule {
        BlockType ore;
        int minY;
        int maxY;
        float cellChance; // chance that a 2x2x2 cell holds a cluster
    };
    static const OreRule* oreRules(int& count);

private:
    struct CaveSamples;

    void fillColumns(ChunkPos pos, Chunk& out, const ColumnInfo* columns) const;
    void carveCaves(ChunkPos pos, Chunk& out, const ColumnInfo* columns) const;
    void placeOres(ChunkPos pos, Chunk& out) const;
    void placeTrees(ChunkPos pos, Chunk& out) const;

    std::uint64_t m_seed;
    PerlinNoise m_temperature;
    PerlinNoise m_humidity;
    PerlinNoise m_continental;
    PerlinNoise m_hills;
    PerlinNoise m_mountains;
    PerlinNoise m_caveA;
    PerlinNoise m_caveB;
    PerlinNoise m_cavern;
    PerlinNoise m_entrances;
};

} // namespace voxelforge
