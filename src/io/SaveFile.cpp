#include "io/SaveFile.hpp"

#include "core/Hash.hpp"
#include "io/BinaryStream.hpp"
#include "io/Rle.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <system_error>

namespace voxelforge {
namespace {

constexpr std::uint8_t kMagic[4] = {'V', 'X', 'F', 'G'};
constexpr std::size_t kHeaderSize = 4 + 4 + 4 + 8;
constexpr std::uint32_t kMaxChunks = 1u << 20;

bool finite(float v) { return std::isfinite(v); }

} // namespace

const char* saveStatusMessage(SaveStatus status) {
    switch (status) {
    case SaveStatus::Ok: return "ok";
    case SaveStatus::FileNotFound: return "save file not found";
    case SaveStatus::IoError: return "could not read or write the save file";
    case SaveStatus::BadMagic: return "not a VoxelForge save file";
    case SaveStatus::UnsupportedVersion: return "save file version is not supported";
    case SaveStatus::Truncated: return "save file is truncated";
    case SaveStatus::ChecksumMismatch: return "save file checksum mismatch (corrupted)";
    case SaveStatus::Corrupt: return "save file contents are invalid";
    }
    return "unknown error";
}

SavedChunk encodeChunk(ChunkPos pos, const Chunk& chunk) {
    SavedChunk saved;
    saved.pos = pos;
    rleEncode(chunk.bytes(), saved.rle);
    return saved;
}

bool decodeChunk(std::span<const std::uint8_t> rle, Chunk& out) {
    if (!rleDecode(rle, out.bytes())) {
        return false;
    }
    for (const std::uint8_t b : out.bytes()) {
        if (!isValidBlock(b)) {
            return false;
        }
    }
    return true;
}

std::vector<std::uint8_t> serializeWorldSave(const WorldSave& save) {
    ByteWriter w;
    for (const std::uint8_t c : kMagic) {
        w.u8(c);
    }
    w.u32(kSaveVersion);
    const std::size_t sizeOffset = w.size();
    w.u32(0); // payload size, patched below
    const std::size_t checksumOffset = w.size();
    w.u64(0); // checksum, patched below

    const std::size_t payloadStart = w.size();
    w.u64(save.seed);
    w.f32(save.timeOfDay);
    w.f32(save.player.position.x);
    w.f32(save.player.position.y);
    w.f32(save.player.position.z);
    w.f32(save.player.yaw);
    w.f32(save.player.pitch);
    w.u8(save.player.flying ? 1 : 0);
    w.u8(static_cast<std::uint8_t>(save.player.selectedSlot));
    for (const BlockType b : save.player.hotbar) {
        w.u8(static_cast<std::uint8_t>(b));
    }
    w.u32(static_cast<std::uint32_t>(save.chunks.size()));
    for (const SavedChunk& chunk : save.chunks) {
        w.i32(chunk.pos.x);
        w.i32(chunk.pos.z);
        w.u32(static_cast<std::uint32_t>(chunk.rle.size()));
        w.bytes(chunk.rle);
    }

    const std::span<const std::uint8_t> payload(w.data().data() + payloadStart, w.size() - payloadStart);
    w.patchU32(sizeOffset, static_cast<std::uint32_t>(payload.size()));
    w.patchU64(checksumOffset, fnv1a64(payload));
    return w.take();
}

SaveStatus deserializeWorldSave(std::span<const std::uint8_t> bytes, WorldSave& out) {
    if (bytes.size() < 4) {
        return SaveStatus::Truncated;
    }
    for (std::size_t i = 0; i < 4; ++i) {
        if (bytes[i] != kMagic[i]) {
            return SaveStatus::BadMagic;
        }
    }
    if (bytes.size() < kHeaderSize) {
        return SaveStatus::Truncated;
    }
    ByteReader header(bytes.subspan(4));
    std::uint32_t version = 0;
    std::uint32_t payloadSize = 0;
    std::uint64_t checksum = 0;
    header.u32(version);
    header.u32(payloadSize);
    header.u64(checksum);
    if (version == 0 || version > kSaveVersion) {
        return SaveStatus::UnsupportedVersion;
    }
    const std::span<const std::uint8_t> payload = bytes.subspan(kHeaderSize);
    if (payload.size() < payloadSize) {
        return SaveStatus::Truncated;
    }
    if (payload.size() > payloadSize) {
        return SaveStatus::Corrupt; // trailing garbage
    }
    if (fnv1a64(payload) != checksum) {
        return SaveStatus::ChecksumMismatch;
    }

    WorldSave save;
    ByteReader r(payload);
    std::uint8_t flying = 0;
    std::uint8_t slot = 0;
    bool ok = r.u64(save.seed) && r.f32(save.timeOfDay) && r.f32(save.player.position.x) &&
              r.f32(save.player.position.y) && r.f32(save.player.position.z) && r.f32(save.player.yaw) &&
              r.f32(save.player.pitch) && r.u8(flying) && r.u8(slot);
    for (BlockType& b : save.player.hotbar) {
        std::uint8_t raw = 0;
        ok = ok && r.u8(raw);
        if (ok && (!isValidBlock(raw) || raw == 0)) {
            return SaveStatus::Corrupt;
        }
        b = static_cast<BlockType>(raw);
    }
    std::uint32_t chunkCount = 0;
    ok = ok && r.u32(chunkCount);
    if (!ok) {
        return SaveStatus::Truncated;
    }
    if (!finite(save.timeOfDay) || !finite(save.player.position.x) || !finite(save.player.position.y) ||
        !finite(save.player.position.z) || !finite(save.player.yaw) || !finite(save.player.pitch) ||
        slot >= kSavedHotbarSlots || flying > 1 || chunkCount > kMaxChunks) {
        return SaveStatus::Corrupt;
    }
    save.player.flying = flying != 0;
    save.player.selectedSlot = slot;

    auto scratch = std::make_unique<Chunk>();
    save.chunks.reserve(chunkCount);
    for (std::uint32_t i = 0; i < chunkCount; ++i) {
        SavedChunk chunk;
        std::uint32_t size = 0;
        std::span<const std::uint8_t> data;
        if (!r.i32(chunk.pos.x) || !r.i32(chunk.pos.z) || !r.u32(size) || !r.bytes(size, data)) {
            return SaveStatus::Truncated;
        }
        if (!decodeChunk(data, *scratch)) {
            return SaveStatus::Corrupt;
        }
        chunk.rle.assign(data.begin(), data.end());
        save.chunks.push_back(std::move(chunk));
    }
    if (r.remaining() != 0) {
        return SaveStatus::Corrupt;
    }
    out = std::move(save);
    return SaveStatus::Ok;
}

SaveStatus writeWorldSave(const std::string& path, const WorldSave& save) {
    namespace fs = std::filesystem;
    const std::vector<std::uint8_t> bytes = serializeWorldSave(save);
    std::error_code ec;
    const fs::path target(path);
    if (target.has_parent_path()) {
        fs::create_directories(target.parent_path(), ec);
        if (ec) {
            return SaveStatus::IoError;
        }
    }
    const fs::path temp = target.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return SaveStatus::IoError;
        }
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            return SaveStatus::IoError;
        }
    }
    fs::rename(temp, target, ec);
    if (ec) {
        fs::remove(temp, ec);
        return SaveStatus::IoError;
    }
    return SaveStatus::Ok;
}

SaveStatus readWorldSave(const std::string& path, WorldSave& out) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return SaveStatus::FileNotFound;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return SaveStatus::IoError;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) {
        return SaveStatus::IoError;
    }
    return deserializeWorldSave(bytes, out);
}

} // namespace voxelforge
