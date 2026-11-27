#pragma once

#include <array>
#include <cstdint>

namespace voxelforge {

struct FbmParams {
    int octaves = 4;
    float frequency = 1.0f / 64.0f;
    float lacunarity = 2.0f; // frequency multiplier per octave
    float gain = 0.5f;       // amplitude multiplier per octave (persistence)
};

// Ken Perlin's "improved" gradient noise (2002) with a seeded permutation table.
// Instances are immutable after construction, so all queries are thread safe.
class PerlinNoise {
public:
    explicit PerlinNoise(std::uint64_t seed);

    // Single octave noise in [-1, 1]. Integer lattice points always return 0.
    float noise2(float x, float y) const;
    float noise3(float x, float y, float z) const;

    // Fractal Brownian motion: octaves of noise summed with decreasing amplitude and
    // increasing frequency; normalised back to [-1, 1].
    float fbm2(float x, float y, const FbmParams& params) const;
    float fbm3(float x, float y, float z, const FbmParams& params) const;

    // Ridged multifractal in [0, 1]: sharp crests where the noise crosses zero, used for
    // mountain ranges.
    float ridged2(float x, float y, const FbmParams& params) const;

private:
    std::array<std::uint8_t, 512> m_perm{};
};

} // namespace voxelforge
