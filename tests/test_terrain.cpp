#include "doctest.h"

#include "world/TerrainGenerator.hpp"

#include <memory>

using namespace voxelforge;

namespace {
std::uint64_t chunkHash(const TerrainGenerator& gen, ChunkPos pos) {
    auto chunk = std::make_unique<Chunk>();
    gen.generate(pos, *chunk);
    return chunk->contentHash();
}
} // namespace

TEST_CASE("terrain: same seed reproduces identical chunks") {
    const TerrainGenerator a(20241127);
    const TerrainGenerator b(20241127);
    for (int cz = -3; cz <= 3; cz += 2) {
        for (int cx = -3; cx <= 3; cx += 2) {
            CHECK(chunkHash(a, {cx, cz}) == chunkHash(b, {cx, cz}));
        }
    }
}

TEST_CASE("terrain: generation order does not matter") {
    const TerrainGenerator gen(77);
    const std::uint64_t first = chunkHash(gen, {5, -9});
    for (int i = -2; i <= 2; ++i) {
        (void)chunkHash(gen, {i, i * 3});
    }
    CHECK(chunkHash(gen, {5, -9}) == first);
}

TEST_CASE("terrain: different seeds produce different worlds") {
    const TerrainGenerator a(1);
    const TerrainGenerator b(2);
    int differing = 0;
    for (int cx = 0; cx < 6; ++cx) {
        differing += chunkHash(a, {cx, 0}) != chunkHash(b, {cx, 0}) ? 1 : 0;
    }
    CHECK(differing >= 5);
}

TEST_CASE("terrain: structural invariants") {
    const TerrainGenerator gen(4242);
    auto chunk = std::make_unique<Chunk>();
    for (int cx = -4; cx < 4; ++cx) {
        gen.generate({cx, cx * 2}, *chunk);
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                CHECK(chunk->get(x, 0, z) == BlockType::Bedrock);
                CHECK(chunk->get(x, kChunkHeight - 1, z) == BlockType::Air);
                // Nothing but air or water directly above the surface up to sea level.
                const int h = gen.surfaceHeight(ChunkPos{cx, cx * 2}.originX() + x, ChunkPos{cx, cx * 2}.originZ() + z);
                CHECK(h > 0);
                CHECK(h < kChunkHeight - 1);
            }
        }
    }
}

TEST_CASE("terrain: oceans are filled with water up to sea level") {
    const TerrainGenerator gen(4242);
    // Find a column below sea level and check that it is flooded.
    bool foundOcean = false;
    for (int wx = -2000; wx < 2000 && !foundOcean; wx += 7) {
        const int h = gen.surfaceHeight(wx, 0);
        if (h < kSeaLevel - 2) {
            const ChunkPos pos = ChunkPos::fromWorld(wx, 0);
            auto chunk = std::make_unique<Chunk>();
            gen.generate(pos, *chunk);
            const int lx = wx - pos.originX();
            CHECK(chunk->get(lx, kSeaLevel, 0) == BlockType::Water);
            CHECK(chunk->get(lx, kSeaLevel + 1, 0) == BlockType::Air);
            foundOcean = true;
        }
    }
    CHECK(foundOcean);
}
