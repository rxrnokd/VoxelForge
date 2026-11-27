#include "doctest.h"

#include "core/Hash.hpp"
#include "world/Noise.hpp"

#include <cmath>

using namespace voxelforge;

TEST_CASE("noise: same seed gives identical values") {
    const PerlinNoise a(1234);
    const PerlinNoise b(1234);
    for (int i = 0; i < 1000; ++i) {
        const float x = static_cast<float>(i) * 0.731f - 300.0f;
        const float y = static_cast<float>(i) * 0.377f + 51.0f;
        CHECK(a.noise2(x, y) == b.noise2(x, y));
        CHECK(a.noise3(x, y, x * 0.5f) == b.noise3(x, y, x * 0.5f));
    }
}

TEST_CASE("noise: different seeds give different fields") {
    const PerlinNoise a(1);
    const PerlinNoise b(2);
    int differences = 0;
    for (int i = 0; i < 200; ++i) {
        const float x = static_cast<float>(i) * 1.37f + 0.5f;
        if (std::fabs(a.noise2(x, 3.3f) - b.noise2(x, 3.3f)) > 1e-4f) {
            ++differences;
        }
    }
    CHECK(differences > 150);
}

TEST_CASE("noise: values stay in [-1, 1] and cover a reasonable range") {
    const PerlinNoise n(99);
    float lo2 = 1.0f, hi2 = -1.0f, lo3 = 1.0f, hi3 = -1.0f;
    SplitMix64 rng(7);
    for (int i = 0; i < 200000; ++i) {
        const float x = rng.nextFloat() * 1000.0f - 500.0f;
        const float y = rng.nextFloat() * 1000.0f - 500.0f;
        const float z = rng.nextFloat() * 1000.0f - 500.0f;
        const float v2 = n.noise2(x, y);
        const float v3 = n.noise3(x, y, z);
        lo2 = std::min(lo2, v2); hi2 = std::max(hi2, v2);
        lo3 = std::min(lo3, v3); hi3 = std::max(hi3, v3);
    }
    CHECK(lo2 >= -1.0f);
    CHECK(hi2 <= 1.0f);
    CHECK(lo3 >= -1.0f);
    CHECK(hi3 <= 1.0f);
    CHECK(hi2 - lo2 > 1.2f);
    CHECK(hi3 - lo3 > 1.2f);
}

TEST_CASE("noise: zero at integer lattice points and continuous between them") {
    const PerlinNoise n(5);
    CHECK(n.noise2(3.0f, -7.0f) == doctest::Approx(0.0f));
    CHECK(n.noise3(3.0f, -7.0f, 11.0f) == doctest::Approx(0.0f));
    // Small steps produce small changes (no discontinuities at cell borders).
    float prev = n.noise2(-4.0f, 2.5f);
    for (int i = 1; i <= 8000; ++i) {
        const float x = -4.0f + static_cast<float>(i) * 0.001f;
        const float v = n.noise2(x, 2.5f);
        CHECK(std::fabs(v - prev) < 0.01f);
        prev = v;
    }
}

TEST_CASE("noise: fbm and ridged ranges") {
    const PerlinNoise n(42);
    const FbmParams params{5, 1.0f / 50.0f, 2.0f, 0.5f};
    for (int i = 0; i < 5000; ++i) {
        const float x = static_cast<float>(i) * 3.1f;
        const float f = n.fbm2(x, -x * 0.7f, params);
        const float f3 = n.fbm3(x, 12.0f, -x * 0.7f, params);
        const float r = n.ridged2(x, x * 0.3f, params);
        CHECK(f >= -1.0f);
        CHECK(f <= 1.0f);
        CHECK(f3 >= -1.0f);
        CHECK(f3 <= 1.0f);
        CHECK(r >= 0.0f);
        CHECK(r <= 1.0f);
    }
}
