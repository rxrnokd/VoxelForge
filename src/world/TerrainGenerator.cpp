#include "world/TerrainGenerator.hpp"

#include "core/Hash.hpp"

#include <algorithm>

namespace voxelforge {
namespace {
constexpr FbmParams kContinental{4, 1.0f / 512.0f, 2.0f, 0.5f};
constexpr FbmParams kDetail{5, 1.0f / 96.0f, 2.0f, 0.5f};
} // namespace

TerrainGenerator::TerrainGenerator(std::uint64_t seed)
    : m_seed(seed), m_continental(mix64(seed ^ 0x1001)), m_detail(mix64(seed ^ 0x1002)) {}

int TerrainGenerator::surfaceHeight(int wx, int wz) const {
    const float x = static_cast<float>(wx);
    const float z = static_cast<float>(wz);
    const float continental = m_continental.fbm2(x, z, kContinental);
    const float detail = m_detail.fbm2(x, z, kDetail);
    const float h = 66.0f + continental * 28.0f + detail * 10.0f;
    return std::clamp(static_cast<int>(h), 1, kChunkHeight - 2);
}

void TerrainGenerator::generate(ChunkPos pos, Chunk& out) const {
    out.fill(BlockType::Air);
    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int lx = 0; lx < kChunkSize; ++lx) {
            const int wx = pos.originX() + lx;
            const int wz = pos.originZ() + lz;
            const int height = surfaceHeight(wx, wz);
            const bool beach = height <= kSeaLevel + 1;

            out.set(lx, 0, lz, BlockType::Bedrock);
            for (int y = 1; y <= height; ++y) {
                BlockType block = BlockType::Stone;
                if (y == height) {
                    block = beach ? BlockType::Sand : BlockType::Grass;
                } else if (y > height - 4) {
                    block = beach ? BlockType::Sand : BlockType::Dirt;
                }
                out.set(lx, y, lz, block);
            }
            for (int y = height + 1; y <= kSeaLevel; ++y) {
                out.set(lx, y, lz, BlockType::Water);
            }
        }
    }
}

} // namespace voxelforge
