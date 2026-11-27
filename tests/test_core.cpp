#include "doctest.h"

#include "core/CommandLine.hpp"
#include "core/Math.hpp"

#include <array>

using namespace voxelforge;

namespace {
template <std::size_t N>
CommandLineResult parse(const std::array<const char*, N>& args) {
    return parseCommandLine(static_cast<int>(N), args.data());
}
} // namespace

TEST_CASE("command line: defaults") {
    const auto r = parse(std::array{"voxelforge"});
    REQUIRE(r.ok());
    CHECK_FALSE(r.options.seed.has_value());
    CHECK(r.options.screenshotPath.empty());
    CHECK_FALSE(r.options.benchmark);
}

TEST_CASE("command line: full option set") {
    const auto r = parse(std::array{"voxelforge", "--seed", "1234", "--render-distance", "12", "--time", "0.25",
                                    "--pos", "1.5,80,-3", "--yaw", "45", "--pitch", "-20", "--screenshot",
                                    "out.png", "--frames", "5", "--world", "alpha", "--new", "--no-save"});
    REQUIRE(r.ok());
    CHECK(*r.options.seed == 1234u);
    CHECK(*r.options.renderDistance == 12);
    CHECK(*r.options.timeOfDay == doctest::Approx(0.25f));
    CHECK(r.options.position->x == doctest::Approx(1.5f));
    CHECK(r.options.position->y == doctest::Approx(80.0f));
    CHECK(r.options.position->z == doctest::Approx(-3.0f));
    CHECK(*r.options.yawDegrees == doctest::Approx(45.0f));
    CHECK(*r.options.pitchDegrees == doctest::Approx(-20.0f));
    CHECK(r.options.screenshotPath == "out.png");
    CHECK(r.options.screenshotFrames == 5);
    CHECK(r.options.worldName == "alpha");
    CHECK(r.options.freshWorld);
    CHECK(r.options.noSave);
}

TEST_CASE("command line: rejects bad input") {
    CHECK_FALSE(parse(std::array{"voxelforge", "--seed"}).ok());
    CHECK_FALSE(parse(std::array{"voxelforge", "--seed", "abc"}).ok());
    CHECK_FALSE(parse(std::array{"voxelforge", "--render-distance", "100"}).ok());
    CHECK_FALSE(parse(std::array{"voxelforge", "--time", "1.5"}).ok());
    CHECK_FALSE(parse(std::array{"voxelforge", "--pos", "1,2"}).ok());
    CHECK_FALSE(parse(std::array{"voxelforge", "--world", "../evil"}).ok());
    CHECK_FALSE(parse(std::array{"voxelforge", "--bogus"}).ok());
}

TEST_CASE("math: floorDiv / floorMod handle negatives") {
    CHECK(floorDiv(15, 16) == 0);
    CHECK(floorDiv(16, 16) == 1);
    CHECK(floorDiv(-1, 16) == -1);
    CHECK(floorDiv(-16, 16) == -1);
    CHECK(floorDiv(-17, 16) == -2);
    CHECK(floorMod(-1, 16) == 15);
    CHECK(floorMod(-16, 16) == 0);
    CHECK(floorMod(17, 16) == 1);
    CHECK(fastFloor(-0.5f) == -1);
    CHECK(fastFloor(2.0f) == 2);
}

TEST_CASE("math: matrix inverse and projection") {
    const Mat4 proj = Mat4::perspective(1.2f, 16.0f / 9.0f, 0.1f, 500.0f);
    const Mat4 view = Mat4::lookAt({3, 4, 5}, {0.3f, -0.2f, -1.0f}, {0, 1, 0});
    const Mat4 vp = proj * view;
    const Mat4 id = vp * vp.inverse();
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            CHECK(id.at(r, c) == doctest::Approx(r == c ? 1.0f : 0.0f).epsilon(1e-3));
        }
    }
    // A point straight ahead of the camera ends up in the middle of clip space.
    const Mat4 v2 = Mat4::lookAt({0, 0, 0}, {0, 0, -1}, {0, 1, 0});
    const auto clip = (proj * v2).transform({0, 0, -10});
    CHECK(clip[0] / clip[3] == doctest::Approx(0.0f));
    CHECK(clip[1] / clip[3] == doctest::Approx(0.0f));
    CHECK(clip[2] / clip[3] > -1.0f);
    CHECK(clip[2] / clip[3] < 1.0f);
}

TEST_CASE("math: yaw/pitch direction convention") {
    const Vec3 forward = directionFromYawPitch(0.0f, 0.0f);
    CHECK(forward.z == doctest::Approx(-1.0f));
    const Vec3 right = directionFromYawPitch(kPi / 2.0f, 0.0f);
    CHECK(right.x == doctest::Approx(1.0f));
    const Vec3 up = directionFromYawPitch(0.0f, kPi / 2.0f);
    CHECK(up.y == doctest::Approx(1.0f));
}
