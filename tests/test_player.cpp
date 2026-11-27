#include "doctest.h"

#include "TestWorld.hpp"
#include "player/Player.hpp"

using namespace voxelforge;
using voxelforge::test::TestWorld;

namespace {
constexpr float kDt = 1.0f / 60.0f;

void simulate(Player& p, const VoxelQuery& w, const MoveInput& in, float seconds) {
    const int steps = static_cast<int>(seconds / kDt);
    for (int i = 0; i < steps; ++i) {
        p.step(w, in, kDt);
    }
}
} // namespace

TEST_CASE("player: falls under gravity and lands on the ground") {
    TestWorld world(64);
    Player p;
    p.setPosition({0.5f, 80.0f, 0.5f});
    simulate(p, world, {}, 3.0f);
    CHECK(p.onGround());
    CHECK(p.position().y == doctest::Approx(64.0f));
    CHECK(p.velocity().y == doctest::Approx(0.0f));
}

TEST_CASE("player: jump height is a bit more than one block") {
    TestWorld world(64);
    Player p;
    p.setPosition({0.5f, 64.0f, 0.5f});
    simulate(p, world, {}, 0.2f);
    REQUIRE(p.onGround());
    MoveInput jump;
    jump.jump = true;
    p.step(world, jump, kDt);
    float peak = p.position().y;
    for (int i = 0; i < 120; ++i) {
        p.step(world, {}, kDt);
        peak = std::max(peak, p.position().y);
    }
    CHECK(peak - 64.0f > 1.1f);
    CHECK(peak - 64.0f < 1.4f);
    CHECK(p.onGround());
}

TEST_CASE("player: can jump onto a one block step but not a two block wall") {
    TestWorld world(64);
    for (int z = -3; z <= 3; ++z) world.set(2, 64, z, BlockType::Stone);
    for (int z = -3; z <= 3; ++z) {
        world.set(6, 65, z, BlockType::Stone);
        world.set(6, 66, z, BlockType::Stone);
        world.set(6, 65 - 1, z, BlockType::Stone);
    }
    Player p;
    p.setPosition({0.5f, 64.0f, 0.5f});
    p.setLook(kPi / 2.0f, 0.0f); // facing +X
    MoveInput in;
    in.forward = 1.0f;
    in.jump = true;
    simulate(p, world, in, 1.5f);
    CHECK(p.position().y >= 65.0f); // climbed the step
    simulate(p, world, in, 2.0f);
    CHECK(p.position().x < 6.0f); // blocked by the high wall
}

TEST_CASE("player: walking speed and direction follow yaw") {
    TestWorld world(64);
    Player p;
    p.setPosition({0.5f, 64.0f, 0.5f});
    p.setLook(0.0f, 0.0f); // facing -Z
    MoveInput in;
    in.forward = 1.0f;
    simulate(p, world, in, 2.0f);
    CHECK(p.position().z < -6.0f);
    CHECK(std::fabs(p.position().x - 0.5f) < 1e-3f);
    CHECK(std::fabs(p.velocity().z) == doctest::Approx(Player::kWalkSpeed));
}

TEST_CASE("player: pitch is clamped") {
    Player p;
    p.addLook(0.0f, 10.0f);
    CHECK(p.pitch() == doctest::Approx(Player::kMaxPitch));
    p.addLook(0.0f, -20.0f);
    CHECK(p.pitch() == doctest::Approx(-Player::kMaxPitch));
}

TEST_CASE("player: flying ignores gravity, swimming slows the fall") {
    TestWorld world(10);
    Player p;
    p.setPosition({0.5f, 50.0f, 0.5f});
    p.setFlying(true);
    simulate(p, world, {}, 1.0f);
    CHECK(p.position().y == doctest::Approx(50.0f));

    TestWorld pool(10);
    for (int y = 10; y < 30; ++y)
        for (int x = -2; x <= 2; ++x)
            for (int z = -2; z <= 2; ++z)
                pool.set(x, y, z, BlockType::Water);
    Player swimmer;
    swimmer.setPosition({0.5f, 25.0f, 0.5f});
    simulate(swimmer, pool, {}, 0.5f);
    CHECK(swimmer.inWater());
    CHECK(swimmer.velocity().y >= -Player::kSwimSpeed - 1e-3f);
}
