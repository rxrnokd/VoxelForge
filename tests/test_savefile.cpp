#include "doctest.h"

#include "io/SaveFile.hpp"
#include "world/TerrainGenerator.hpp"

#include <filesystem>
#include <memory>

using namespace voxelforge;

namespace {

WorldSave sampleSave() {
    WorldSave save;
    save.seed = 0xDEADBEEFCAFEull;
    save.timeOfDay = 0.37f;
    save.player.position = {12.5f, 80.0f, -301.25f};
    save.player.yaw = 1.25f;
    save.player.pitch = -0.5f;
    save.player.flying = true;
    save.player.selectedSlot = 4;
    for (int i = 0; i < kSavedHotbarSlots; ++i) {
        save.player.hotbar[static_cast<std::size_t>(i)] = static_cast<BlockType>(1 + i);
    }
    const TerrainGenerator gen(save.seed);
    for (int i = 0; i < 3; ++i) {
        auto chunk = std::make_unique<Chunk>();
        gen.generate({i, -i}, *chunk);
        chunk->set(1, 200, 1, BlockType::Brick);
        save.chunks.push_back(encodeChunk({i, -i}, *chunk));
    }
    return save;
}

} // namespace

TEST_CASE("save: serialize / deserialize round trip") {
    const WorldSave save = sampleSave();
    const std::vector<std::uint8_t> bytes = serializeWorldSave(save);
    REQUIRE(bytes.size() > 20);
    CHECK(bytes[0] == 'V');
    CHECK(bytes[3] == 'G');
    CHECK(bytes[4] == kSaveVersion); // little-endian version follows the magic

    WorldSave back;
    REQUIRE(deserializeWorldSave(bytes, back) == SaveStatus::Ok);
    CHECK(back.seed == save.seed);
    CHECK(back.timeOfDay == doctest::Approx(save.timeOfDay));
    CHECK(back.player.position == save.player.position);
    CHECK(back.player.yaw == doctest::Approx(1.25f));
    CHECK(back.player.flying);
    CHECK(back.player.selectedSlot == 4);
    CHECK(back.player.hotbar == save.player.hotbar);
    REQUIRE(back.chunks.size() == 3);
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(back.chunks[i].pos == save.chunks[i].pos);
        auto chunk = std::make_unique<Chunk>();
        REQUIRE(decodeChunk(back.chunks[i].rle, *chunk));
        CHECK(chunk->get(1, 200, 1) == BlockType::Brick);
    }
}

TEST_CASE("save: rejects foreign, future, truncated and corrupted files") {
    const std::vector<std::uint8_t> good = serializeWorldSave(sampleSave());
    WorldSave out;

    std::vector<std::uint8_t> badMagic = good;
    badMagic[0] = 'X';
    CHECK(deserializeWorldSave(badMagic, out) == SaveStatus::BadMagic);

    std::vector<std::uint8_t> future = good;
    future[4] = static_cast<std::uint8_t>(kSaveVersion + 1);
    CHECK(deserializeWorldSave(future, out) == SaveStatus::UnsupportedVersion);

    std::vector<std::uint8_t> truncated(good.begin(), good.end() - 10);
    CHECK(deserializeWorldSave(truncated, out) == SaveStatus::Truncated);
    CHECK(deserializeWorldSave(std::vector<std::uint8_t>(good.begin(), good.begin() + 6), out) == SaveStatus::Truncated);

    std::vector<std::uint8_t> flipped = good;
    flipped[flipped.size() / 2] ^= 0x40;
    CHECK(deserializeWorldSave(flipped, out) == SaveStatus::ChecksumMismatch);

    std::vector<std::uint8_t> trailing = good;
    trailing.push_back(0);
    CHECK(deserializeWorldSave(trailing, out) == SaveStatus::Corrupt);
}

TEST_CASE("save: chunk data with unknown block ids is rejected") {
    auto chunk = std::make_unique<Chunk>();
    chunk->bytes()[100] = 250; // not a block id
    const SavedChunk saved = encodeChunk({0, 0}, *chunk);
    auto out = std::make_unique<Chunk>();
    CHECK_FALSE(decodeChunk(saved.rle, *out));
}

TEST_CASE("save: atomic file write and read back") {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "voxelforge_test_saves";
    fs::remove_all(dir);
    const std::string path = (dir / "nested" / "world.vxf").string();
    const WorldSave save = sampleSave();
    REQUIRE(writeWorldSave(path, save) == SaveStatus::Ok);
    CHECK(fs::exists(path));
    CHECK_FALSE(fs::exists(path + ".tmp"));
    WorldSave back;
    REQUIRE(readWorldSave(path, back) == SaveStatus::Ok);
    CHECK(back.chunks.size() == save.chunks.size());
    // Overwriting keeps a valid file.
    REQUIRE(writeWorldSave(path, back) == SaveStatus::Ok);
    CHECK(readWorldSave(path, back) == SaveStatus::Ok);
    CHECK(readWorldSave((dir / "missing.vxf").string(), back) == SaveStatus::FileNotFound);
    fs::remove_all(dir);
}
