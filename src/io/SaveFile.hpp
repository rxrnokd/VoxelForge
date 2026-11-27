#pragma once

#include "core/Math.hpp"
#include "world/Chunk.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace voxelforge {

// Binary save file layout (all integers little-endian):
//
//   header   magic "VXFG" | u32 version | u32 payload size | u64 FNV-1a(payload)
//   payload  u64 seed | f32 time of day
//            player: f32 x, y, z, yaw, pitch | u8 flying | u8 selected slot | u8 hotbar[9]
//            u32 chunk count
//            per chunk: i32 cx | i32 cz | u32 size | RLE bytes (16 x 256 x 16 block ids)
//
// Only chunks the player modified are stored; everything else is regenerated from the
// seed. The checksum and strict bounds checks reject truncated or corrupted files.

inline constexpr std::uint32_t kSaveVersion = 1;
inline constexpr int kSavedHotbarSlots = 9;

struct SavedChunk {
    ChunkPos pos;
    std::vector<std::uint8_t> rle; // run-length encoded block ids
};

struct PlayerState {
    Vec3 position; // feet
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool flying = false;
    int selectedSlot = 0;
    std::array<BlockType, kSavedHotbarSlots> hotbar{};
};

struct WorldSave {
    std::uint64_t seed = 0;
    float timeOfDay = 0.0f;
    PlayerState player;
    std::vector<SavedChunk> chunks;
};

enum class SaveStatus {
    Ok,
    FileNotFound,
    IoError,
    BadMagic,
    UnsupportedVersion,
    Truncated,
    ChecksumMismatch,
    Corrupt,
};
const char* saveStatusMessage(SaveStatus status);

SavedChunk encodeChunk(ChunkPos pos, const Chunk& chunk);
// Decodes and validates (every byte must be a known block id).
bool decodeChunk(std::span<const std::uint8_t> rle, Chunk& out);

std::vector<std::uint8_t> serializeWorldSave(const WorldSave& save);
SaveStatus deserializeWorldSave(std::span<const std::uint8_t> bytes, WorldSave& out);

// Writes atomically (temporary file + rename) so a crash never leaves a half-written save.
SaveStatus writeWorldSave(const std::string& path, const WorldSave& save);
SaveStatus readWorldSave(const std::string& path, WorldSave& out);

} // namespace voxelforge
