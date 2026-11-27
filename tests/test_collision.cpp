#include "doctest.h"

#include "TestWorld.hpp"
#include "physics/Collision.hpp"

using namespace voxelforge;
using voxelforge::test::TestWorld;

TEST_CASE("aabb: intersection excludes touching boxes") {
    const AABB a{{0, 0, 0}, {1, 1, 1}};
    CHECK(a.intersects(AABB{{0.5f, 0.5f, 0.5f}, {2, 2, 2}}));
    CHECK_FALSE(a.intersects(AABB{{1, 0, 0}, {2, 1, 1}}));
    CHECK_FALSE(a.intersects(AABB{{0, 1.01f, 0}, {1, 2, 1}}));
    const AABB feet = AABB::fromFeet({5, 10, 5}, 0.6f, 1.8f);
    CHECK(feet.min.x == doctest::Approx(4.7f));
    CHECK(feet.max.y == doctest::Approx(11.8f));
}

TEST_CASE("collision: falling box lands exactly on the ground") {
    TestWorld world(64);
    AABB box = AABB::fromFeet({0.5f, 70.0f, 0.5f}, 0.6f, 1.8f);
    const CollisionResult r = moveAndCollide(world, box, {0, -10.0f, 0});
    CHECK(r.hitY);
    CHECK(r.landed);
    CHECK(box.min.y == doctest::Approx(64.0f));
    CHECK(r.applied.y == doctest::Approx(-6.0f));
}

TEST_CASE("collision: huge displacement cannot tunnel through a thin floor") {
    TestWorld world(0);
    for (int x = -2; x <= 2; ++x)
        for (int z = -2; z <= 2; ++z)
            world.set(x, 10, z, BlockType::Glass);
    AABB box = AABB::fromFeet({0.5f, 50.0f, 0.5f}, 0.6f, 1.8f);
    moveAndCollide(world, box, {0, -500.0f, 0});
    CHECK(box.min.y == doctest::Approx(11.0f));
}

TEST_CASE("collision: walls stop horizontal motion and allow sliding") {
    TestWorld world(64);
    for (int z = -5; z <= 5; ++z) {
        world.set(3, 64, z, BlockType::Stone);
        world.set(3, 65, z, BlockType::Stone);
    }
    AABB box = AABB::fromFeet({1.5f, 64.0f, 0.5f}, 0.6f, 1.8f);
    const CollisionResult r = moveAndCollide(world, box, {2.0f, 0.0f, 1.0f});
    CHECK(r.hitX);
    CHECK_FALSE(r.hitZ);
    CHECK(box.max.x == doctest::Approx(3.0f));
    CHECK(box.min.z == doctest::Approx(1.2f)); // slid along the wall
}

TEST_CASE("collision: ceilings stop upward motion") {
    TestWorld world(64);
    world.set(0, 66, 0, BlockType::Stone);
    AABB box = AABB::fromFeet({0.5f, 64.0f, 0.5f}, 0.6f, 1.8f);
    const CollisionResult r = moveAndCollide(world, box, {0, 1.0f, 0});
    CHECK(r.hitY);
    CHECK_FALSE(r.landed);
    CHECK(box.max.y == doctest::Approx(66.0f));
}

TEST_CASE("collision: water and air do not collide, glass does") {
    TestWorld world(64);
    world.set(1, 64, 0, BlockType::Water);
    world.set(-1, 64, 0, BlockType::Glass);
    AABB box = AABB::fromFeet({0.5f, 64.0f, 0.5f}, 0.6f, 1.8f);
    CHECK_FALSE(overlapsSolid(world, box));
    CHECK(moveAndCollide(world, box, {1.0f, 0, 0}).hitX == false);
    AABB box2 = AABB::fromFeet({0.5f, 64.0f, 0.5f}, 0.6f, 1.8f);
    CHECK(moveAndCollide(world, box2, {-1.0f, 0, 0}).hitX);
    CHECK(overlapsSolid(world, AABB::fromFeet({0.5f, 63.5f, 0.5f}, 0.6f, 1.8f)));
}
