#include "doctest.h"

#include "TestWorld.hpp"
#include "physics/Raycast.hpp"
#include "player/Inventory.hpp"

using namespace voxelforge;
using voxelforge::test::TestWorld;

TEST_CASE("raycast: looking straight down hits the ground top face") {
    TestWorld world(64);
    const auto hit = raycastBlocks(world, {0.5f, 66.0f, 0.5f}, {0, -1, 0}, 10.0f);
    REQUIRE(hit);
    CHECK(hit->block == IVec3{0, 63, 0});
    CHECK(hit->normal == IVec3{0, 1, 0});
    CHECK(hit->distance == doctest::Approx(2.0f));
    CHECK(hit->placementTarget() == IVec3{0, 64, 0});
}

TEST_CASE("raycast: axis aligned rays in every direction report the entered face") {
    TestWorld world(0);
    const Vec3 origin{0.5f, 10.5f, 0.5f};
    const IVec3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (const IVec3& d : dirs) {
        const IVec3 target{d.x * 3, 10 + d.y * 3, d.z * 3};
        world.set(target.x, target.y, target.z, BlockType::Stone);
        const auto hit = raycastBlocks(world, origin, {static_cast<float>(d.x), static_cast<float>(d.y),
                                                       static_cast<float>(d.z)}, 8.0f);
        REQUIRE(hit);
        CHECK(hit->block == target);
        CHECK(hit->normal == IVec3{-d.x, -d.y, -d.z});
        CHECK(hit->distance == doctest::Approx(2.5f));
    }
}

TEST_CASE("raycast: diagonal ray visits cells in order and finds the first block") {
    TestWorld world(0);
    world.set(3, 3, 0, BlockType::Brick);
    world.set(5, 5, 0, BlockType::Brick);
    const auto hit = raycastBlocks(world, {0.2f, 0.1f, 0.5f}, {1.0f, 1.0f, 0.0f}, 20.0f);
    REQUIRE(hit);
    CHECK(hit->block == IVec3{3, 3, 0});
    // Entered either through the -X or -Y face.
    CHECK((hit->normal == IVec3{-1, 0, 0} || hit->normal == IVec3{0, -1, 0}));
}

TEST_CASE("raycast: respects maximum distance and ignores water") {
    TestWorld world(0);
    world.set(10, 5, 0, BlockType::Stone);
    CHECK_FALSE(raycastBlocks(world, {0.5f, 5.5f, 0.5f}, {1, 0, 0}, 5.0f));
    CHECK(raycastBlocks(world, {0.5f, 5.5f, 0.5f}, {1, 0, 0}, 12.0f));

    for (int x = 1; x < 4; ++x) world.set(x, 5, 0, BlockType::Water);
    world.set(4, 5, 0, BlockType::Sand);
    const auto hit = raycastBlocks(world, {0.5f, 5.5f, 0.5f}, {1, 0, 0}, 12.0f);
    REQUIRE(hit);
    CHECK(hit->type == BlockType::Sand);
}

TEST_CASE("raycast: negative coordinates and starting inside a block") {
    TestWorld world(0);
    world.set(-3, 2, -7, BlockType::Glass);
    const auto hit = raycastBlocks(world, {-2.5f, 2.5f, -2.5f}, {-0.1f, 0.0f, -1.0f}, 10.0f);
    REQUIRE(hit);
    CHECK(hit->block == IVec3{-3, 2, -7});

    world.set(0, 2, 0, BlockType::Stone);
    const auto inside = raycastBlocks(world, {0.5f, 2.5f, 0.5f}, {0, 0, 1}, 5.0f);
    REQUIRE(inside);
    CHECK(inside->block == IVec3{0, 2, 0});
    CHECK(inside->normal == IVec3{0, 0, 0});
    CHECK(inside->distance == doctest::Approx(0.0f));
}

TEST_CASE("raycast: rays leaving the world vertically terminate") {
    TestWorld world(0);
    CHECK_FALSE(raycastBlocks(world, {0.5f, 250.0f, 0.5f}, {0, 1, 0}, 1000.0f));
    CHECK_FALSE(raycastBlocks(world, {0.5f, 5.0f, 0.5f}, {0, 0, 0}, 10.0f));
}

TEST_CASE("inventory: selection, scrolling and picking") {
    Inventory inv;
    CHECK(inv.selectedSlot() == 0);
    inv.select(4);
    CHECK(inv.selectedSlot() == 4);
    inv.select(12); // ignored
    CHECK(inv.selectedSlot() == 4);
    inv.scroll(-1.0f);
    CHECK(inv.selectedSlot() == 5);
    inv.select(0);
    inv.scroll(1.0f); // wraps to the last slot
    CHECK(inv.selectedSlot() == Inventory::kSlots - 1);
    inv.scroll(-1.0f);
    CHECK(inv.selectedSlot() == 0);

    const BlockType stone = inv.slot(2);
    inv.pick(stone);
    CHECK(inv.selectedSlot() == 2);
    inv.pick(BlockType::DiamondOre); // not present: replaces the current slot
    CHECK(inv.selectedBlock() == BlockType::DiamondOre);
    inv.pick(BlockType::Air);
    CHECK(inv.selectedBlock() == BlockType::DiamondOre);
}
