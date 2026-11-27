#include "doctest.h"

#include "render/Frustum.hpp"

using namespace voxelforge;

namespace {
Frustum lookingDownMinusZ() {
    const Mat4 proj = Mat4::perspective(1.2f, 16.0f / 9.0f, 0.1f, 200.0f);
    const Mat4 view = Mat4::lookAt({0, 0, 0}, {0, 0, -1}, {0, 1, 0});
    return Frustum(proj * view);
}
} // namespace

TEST_CASE("frustum: boxes in front are visible, boxes behind are culled") {
    const Frustum f = lookingDownMinusZ();
    CHECK(f.intersectsBox({-1, -1, -11}, {1, 1, -9}));
    CHECK_FALSE(f.intersectsBox({-1, -1, 9}, {1, 1, 11}));
    CHECK(f.containsPoint({0, 0, -5}));
    CHECK_FALSE(f.containsPoint({0, 0, 5}));
}

TEST_CASE("frustum: far plane and side planes") {
    const Frustum f = lookingDownMinusZ();
    CHECK_FALSE(f.intersectsBox({-1, -1, -400}, {1, 1, -300}));
    CHECK_FALSE(f.intersectsBox({200, -1, -20}, {210, 1, -10}));  // far to the right
    CHECK_FALSE(f.intersectsBox({-1, 100, -20}, {1, 110, -10}));  // far above
    CHECK(f.intersectsBox({-1, -1, -199}, {1, 1, -150}));        // straddles the far plane
}

TEST_CASE("frustum: boxes that straddle a plane or contain the camera are visible") {
    const Frustum f = lookingDownMinusZ();
    CHECK(f.intersectsBox({-5, -5, -5}, {5, 5, 5}));           // contains the camera
    CHECK(f.intersectsBox({5, -1, -12}, {40, 1, -10}));        // partly inside the right edge
    // A chunk-sized column next to the camera but behind it is culled.
    CHECK_FALSE(f.intersectsBox({-8, -64, 1}, {8, 192, 17}));
}
