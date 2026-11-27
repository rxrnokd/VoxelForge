#pragma once

#include "render/ChunkMesh.hpp"
#include "world/Chunk.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace voxelforge {

// Read-only view of a chunk and its eight horizontal neighbours. Neighbours are needed
// to cull faces on chunk borders and to compute ambient occlusion across them.
// Missing neighbours (nullptr) are treated as air.
struct ChunkNeighborhood {
    std::array<const Chunk*, 9> chunks{}; // index (dz + 1) * 3 + (dx + 1), centre = 4

    const Chunk* center() const { return chunks[4]; }
    const Chunk*& at(int dx, int dz) { return chunks[static_cast<std::size_t>((dz + 1) * 3 + (dx + 1))]; }
};

enum class MeshingMode {
    Culled, // one quad per visible face (hidden faces removed) - used for comparison
    Greedy, // visible faces merged into maximal rectangles
};

// Builds chunk meshes. An instance owns reusable scratch buffers, so give every worker
// thread its own mesher; building never allocates once the buffers have grown.
class ChunkMesher {
public:
    ChunkMesher();

    void build(const ChunkNeighborhood& neighborhood, ChunkMesh& out, MeshingMode mode = MeshingMode::Greedy);

    // Padded copy dimensions: one block of border on every side.
    static constexpr int kPadX = kChunkSize + 2;
    static constexpr int kPadZ = kChunkSize + 2;
    static constexpr int kPadY = kChunkHeight + 2;

private:
    void copyNeighborhood(const ChunkNeighborhood& neighborhood, int topY);
    void computeSkyExposure(const ChunkNeighborhood& neighborhood, int topY);
    void meshFace(int face, int maxY, ChunkMesh& out, MeshingMode mode);
    void emitQuad(int face, int slice, int i, int j, int w, int h, std::uint32_t key, ChunkMesh& out) const;

    static constexpr int padIndex(int x, int y, int z) { return ((y + 1) * kPadZ + (z + 1)) * kPadX + (x + 1); }

    std::vector<BlockType> m_padded;  // (16+2) x (256+2) x (16+2) copy of the neighbourhood
    std::vector<std::uint8_t> m_sky;  // 1 where a cell sees the sky (above every opaque block)
    std::vector<std::uint32_t> m_mask; // face keys for one slice
};

} // namespace voxelforge
