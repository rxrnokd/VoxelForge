#include "doctest.h"

#include "world/World.hpp"

#include <set>

using namespace voxelforge;

namespace {

struct Drained {
    std::set<std::pair<int, int>> meshed;
    std::set<std::pair<int, int>> unloaded;
};

void drain(World& world, Drained& d) {
    MeshUpdate update;
    while (world.popMeshUpdate(update)) {
        d.meshed.insert({update.pos.x, update.pos.z});
        world.recycleMesh(std::move(update.mesh));
    }
    ChunkPos pos;
    while (world.popUnloadedChunk(pos)) {
        d.unloaded.insert({pos.x, pos.z});
    }
}

} // namespace

TEST_CASE("world: streams and meshes every chunk within the render distance") {
    World world(1234, 3);
    const Vec3 focus{8.0f, 80.0f, 8.0f};
    REQUIRE(world.waitForArea(focus, 4, 4, 60.0));
    Drained d;
    drain(world, d);
    for (int dz = -4; dz <= 4; ++dz) {
        for (int dx = -4; dx <= 4; ++dx) {
            if (dx * dx + dz * dz <= 4 * 4 + 4) {
                CHECK(d.meshed.contains({dx, dz}));
            }
        }
    }
    const WorldStats stats = world.stats();
    CHECK(stats.loadedChunks >= d.meshed.size());
    CHECK(world.workerThreads() == 3);
}

TEST_CASE("world: streamed chunks match the generator exactly") {
    World world(99, 4);
    REQUIRE(world.waitForArea({0, 80, 0}, 2, 2, 60.0));
    const TerrainGenerator reference(99);
    for (int cx = -2; cx <= 2; ++cx) {
        Chunk expected;
        reference.generate({cx, 1}, expected);
        const auto snapshot = world.chunkSnapshot({cx, 1});
        REQUIRE(snapshot);
        CHECK(snapshot->contentHash() == expected.contentHash());
    }
}

TEST_CASE("world: block edits, re-meshing and copy-on-write snapshots") {
    World world(7, 2);
    const Vec3 focus{0, 80, 0};
    REQUIRE(world.waitForArea(focus, 2, 2, 60.0));
    Drained d;
    drain(world, d);

    const auto before = world.chunkSnapshot({0, 0});
    REQUIRE(before);
    CHECK(world.setBlock(0, 200, 0, BlockType::Brick));
    CHECK_FALSE(world.setBlock(0, 200, 0, BlockType::Brick)); // no change
    CHECK_FALSE(world.setBlock(0, 300, 0, BlockType::Brick)); // out of range
    CHECK(world.getBlock(0, 200, 0) == BlockType::Brick);
    CHECK(before->get(0, 200, 0) == BlockType::Air); // old snapshot untouched

    // The edit is on a chunk corner: four chunks need new meshes.
    d.meshed.clear();
    REQUIRE(world.waitForArea(focus, 2, 2, 60.0));
    drain(world, d);
    CHECK(d.meshed.contains({0, 0}));
    CHECK(d.meshed.contains({-1, 0}));
    CHECK(d.meshed.contains({0, -1}));
    CHECK(d.meshed.contains({-1, -1}));
    CHECK(world.stats().modifiedChunks == 1);
}

TEST_CASE("world: distant chunks unload and modified chunks survive a round trip") {
    World world(31337, 3);
    REQUIRE(world.waitForArea({8, 80, 8}, 3, 3, 60.0));
    REQUIRE(world.setBlock(5, 150, 5, BlockType::Glass));

    // Travel far away: the original area must unload.
    const Vec3 far{8.0f + 30.0f * kChunkSize, 80.0f, 8.0f};
    REQUIRE(world.waitForArea(far, 3, 3, 60.0));
    Drained d;
    drain(world, d);
    CHECK(d.unloaded.contains({0, 0}));
    CHECK_FALSE(world.isChunkLoaded({0, 0}));
    CHECK(world.getBlock(5, 150, 5) == BlockType::Air); // not loaded -> reads as air
    CHECK(world.stats().modifiedChunks == 1);
    CHECK(world.modifiedChunks().size() == 1);

    // Come back: the edit is restored from memory rather than regenerated.
    REQUIRE(world.waitForArea({8, 80, 8}, 3, 3, 60.0));
    CHECK(world.getBlock(5, 150, 5) == BlockType::Glass);
}

TEST_CASE("world: save data round trip restores edits") {
    std::vector<SavedChunk> saved;
    {
        World world(2468, 2);
        REQUIRE(world.waitForArea({0, 80, 0}, 2, 2, 60.0));
        REQUIRE(world.setBlock(3, 180, 4, BlockType::DiamondOre));
        REQUIRE(world.setBlock(-20, 181, 9, BlockType::Planks));
        saved = world.modifiedChunks();
    }
    CHECK(saved.size() == 2);
    World world(2468, 2);
    for (const SavedChunk& chunk : saved) {
        CHECK(world.addModifiedChunk(chunk));
    }
    SavedChunk broken{{50, 50}, {1, 2, 3}};
    CHECK_FALSE(world.addModifiedChunk(broken));
    REQUIRE(world.waitForArea({0, 80, 0}, 2, 2, 60.0));
    CHECK(world.getBlock(3, 180, 4) == BlockType::DiamondOre);
    CHECK(world.getBlock(-20, 181, 9) == BlockType::Planks);
    CHECK(world.stats().modifiedChunks == 2);
}

TEST_CASE("world: affected chunk computation") {
    ChunkPos out[4];
    CHECK(World::chunksAffectedBy(5, 5, out) == 1);
    CHECK(World::chunksAffectedBy(0, 5, out) == 2);
    CHECK(out[1] == ChunkPos{-1, 0});
    CHECK(World::chunksAffectedBy(15, 15, out) == 4);
    CHECK(out[3] == ChunkPos{1, 1});
    CHECK(World::chunksAffectedBy(-1, -16, out) == 4); // local (15, 0) in chunk (-1, -1)
}

TEST_CASE("world: shutting down with work in flight is safe") {
    for (int i = 0; i < 3; ++i) {
        World world(static_cast<std::uint64_t>(i), 4);
        world.update({0, 80, 0}, 12); // queue lots of work and destroy immediately
    }
    CHECK(true);
}
