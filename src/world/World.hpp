#pragma once

#include "render/Mesher.hpp"
#include "world/Chunk.hpp"
#include "world/TerrainGenerator.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace voxelforge {

// Collection of loaded chunks. (Synchronous version; streaming arrives with the thread pool.)
class World {
public:
    explicit World(std::uint64_t seed);

    std::uint64_t seed() const { return m_generator.seed(); }
    const TerrainGenerator& generator() const { return m_generator; }

    BlockType getBlock(int x, int y, int z) const;
    const Chunk* findChunk(ChunkPos pos) const;
    std::size_t loadedChunkCount() const { return m_chunks.size(); }

    // Generates every chunk within `radius` (plus a one chunk border for meshing).
    void generateArea(ChunkPos center, int radius);
    // Builds the mesh of `pos`; returns false when a neighbour is missing.
    bool buildMesh(ChunkPos pos, ChunkMesher& mesher, ChunkMesh& out) const;

private:
    TerrainGenerator m_generator;
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> m_chunks;
};

} // namespace voxelforge
