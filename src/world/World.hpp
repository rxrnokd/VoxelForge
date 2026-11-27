#pragma once

#include "physics/AABB.hpp"
#include "render/Mesher.hpp"
#include "world/Chunk.hpp"
#include "world/TerrainGenerator.hpp"

#include <cstdint>
#include <array>
#include <memory>
#include <unordered_map>

namespace voxelforge {

// Collection of loaded chunks. (Synchronous version; streaming arrives with the thread pool.)
class World : public VoxelQuery {
public:
    explicit World(std::uint64_t seed);

    std::uint64_t seed() const { return m_generator.seed(); }
    const TerrainGenerator& generator() const { return m_generator; }

    BlockType getBlock(int x, int y, int z) const;
    BlockType blockAt(int x, int y, int z) const override { return getBlock(x, y, z); }
    bool isChunkLoaded(ChunkPos pos) const { return findChunk(pos) != nullptr; }

    // A walkable spawn point (feet position) near the given column.
    Vec3 findSpawnPoint(int nearX, int nearZ) const;
    const Chunk* findChunk(ChunkPos pos) const;
    std::size_t loadedChunkCount() const { return m_chunks.size(); }

    // Changes one block. Returns false when the chunk is not loaded or y is out of range.
    bool setBlock(int x, int y, int z, BlockType type);

    // Chunks whose mesh depends on the block at (x, z): its own chunk plus neighbours
    // touching it (border faces and ambient occlusion). Returns the number written.
    static int chunksAffectedBy(int x, int z, ChunkPos out[4]);

    // Generates every chunk within `radius` (plus a one chunk border for meshing).
    void generateArea(ChunkPos center, int radius);
    // Builds the mesh of `pos`; returns false when a neighbour is missing.
    bool buildMesh(ChunkPos pos, ChunkMesher& mesher, ChunkMesh& out) const;

private:
    TerrainGenerator m_generator;
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> m_chunks;
};

} // namespace voxelforge
