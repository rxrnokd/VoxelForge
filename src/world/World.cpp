#include "world/World.hpp"

namespace voxelforge {

World::World(std::uint64_t seed) : m_generator(seed) {}

const Chunk* World::findChunk(ChunkPos pos) const {
    const auto it = m_chunks.find(pos);
    return it != m_chunks.end() ? it->second.get() : nullptr;
}

BlockType World::getBlock(int x, int y, int z) const {
    if (y < 0 || y >= kChunkHeight) {
        return BlockType::Air;
    }
    const ChunkPos pos = ChunkPos::fromWorld(x, z);
    const Chunk* chunk = findChunk(pos);
    return chunk != nullptr ? chunk->get(x - pos.originX(), y, z - pos.originZ()) : BlockType::Air;
}

void World::generateArea(ChunkPos center, int radius) {
    const int r = radius + 1;
    for (int dz = -r; dz <= r; ++dz) {
        for (int dx = -r; dx <= r; ++dx) {
            const ChunkPos pos{center.x + dx, center.z + dz};
            if (!m_chunks.contains(pos)) {
                auto chunk = std::make_unique<Chunk>();
                m_generator.generate(pos, *chunk);
                m_chunks.emplace(pos, std::move(chunk));
            }
        }
    }
}

bool World::buildMesh(ChunkPos pos, ChunkMesher& mesher, ChunkMesh& out) const {
    ChunkNeighborhood n;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const Chunk* c = findChunk({pos.x + dx, pos.z + dz});
            if (c == nullptr) {
                return false;
            }
            n.at(dx, dz) = c;
        }
    }
    mesher.build(n, out);
    return true;
}

} // namespace voxelforge
