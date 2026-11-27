#pragma once

#include "core/Math.hpp"
#include "world/Block.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>

namespace voxelforge {

inline constexpr int kChunkSize = 16;    // blocks along X and Z
inline constexpr int kChunkHeight = 256; // blocks along Y
inline constexpr int kChunkArea = kChunkSize * kChunkSize;
inline constexpr int kChunkVolume = kChunkArea * kChunkHeight;
inline constexpr int kSeaLevel = 62;

// Horizontal chunk coordinate (chunk (1, 0) covers world x in [16, 32)).
struct ChunkPos {
    int x = 0;
    int z = 0;

    constexpr bool operator==(const ChunkPos&) const = default;

    static constexpr ChunkPos fromWorld(int wx, int wz) {
        return {floorDiv(wx, kChunkSize), floorDiv(wz, kChunkSize)};
    }
    constexpr int originX() const { return x * kChunkSize; }
    constexpr int originZ() const { return z * kChunkSize; }
};

struct ChunkPosHash {
    std::size_t operator()(const ChunkPos& p) const noexcept {
        std::uint64_t h = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(p.x)) << 32) |
                          static_cast<std::uint32_t>(p.z);
        h ^= h >> 33;
        h *= 0xFF51AFD7ED558CCDull;
        h ^= h >> 33;
        return static_cast<std::size_t>(h);
    }
};

// Raw block storage of a 16x256x16 column: one byte per block (64 KiB).
// Layout is Y-major (y, then z, then x) so horizontal layers are contiguous, which keeps
// the long runs of air/stone together for RLE compression and cache friendly meshing.
class Chunk {
public:
    using Storage = std::array<BlockType, kChunkVolume>;

    Chunk() { m_blocks.fill(BlockType::Air); }

    static constexpr int index(int x, int y, int z) { return (y << 8) | (z << 4) | x; }
    static constexpr bool inBounds(int x, int y, int z) {
        return x >= 0 && x < kChunkSize && z >= 0 && z < kChunkSize && y >= 0 && y < kChunkHeight;
    }

    BlockType get(int x, int y, int z) const { return m_blocks[static_cast<std::size_t>(index(x, y, z))]; }
    void set(int x, int y, int z, BlockType type) { m_blocks[static_cast<std::size_t>(index(x, y, z))] = type; }

    // Out-of-range Y is treated as air (above) so callers can probe freely.
    BlockType getSafe(int x, int y, int z) const {
        return inBounds(x, y, z) ? get(x, y, z) : BlockType::Air;
    }

    void fill(BlockType type) { m_blocks.fill(type); }

    // Highest Y containing a non-air block, or -1 if the chunk is empty.
    int highestNonAir() const;

    std::span<const std::uint8_t> bytes() const {
        return {reinterpret_cast<const std::uint8_t*>(m_blocks.data()), m_blocks.size()};
    }
    std::span<std::uint8_t> bytes() {
        return {reinterpret_cast<std::uint8_t*>(m_blocks.data()), m_blocks.size()};
    }
    const Storage& storage() const { return m_blocks; }

    // Content hash, used by tests to verify seed reproducibility.
    std::uint64_t contentHash() const;

private:
    Storage m_blocks;
};

static_assert(sizeof(BlockType) == 1, "blocks must stay one byte each");

} // namespace voxelforge
