#include "world/Chunk.hpp"

#include "core/Hash.hpp"

namespace voxelforge {

int Chunk::highestNonAir() const {
    for (int y = kChunkHeight - 1; y >= 0; --y) {
        const BlockType* layer = &m_blocks[static_cast<std::size_t>(index(0, y, 0))];
        for (int i = 0; i < kChunkArea; ++i) {
            if (layer[i] != BlockType::Air) {
                return y;
            }
        }
    }
    return -1;
}

std::uint64_t Chunk::contentHash() const {
    return fnv1a64(bytes());
}

} // namespace voxelforge
