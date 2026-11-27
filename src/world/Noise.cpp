#include "world/Noise.hpp"

#include "core/Hash.hpp"
#include "core/Math.hpp"

#include <cmath>
#include <numeric>

namespace voxelforge {
namespace {

inline float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

// 2D gradients: 8 unit vectors (4 axis aligned, 4 diagonal).
inline float grad2(std::uint8_t hash, float x, float y) {
    constexpr float d = 0.70710678f;
    switch (hash & 7u) {
    case 0: return x;
    case 1: return -x;
    case 2: return y;
    case 3: return -y;
    case 4: return (x + y) * d;
    case 5: return (-x + y) * d;
    case 6: return (x - y) * d;
    default: return (-x - y) * d;
    }
}

// 3D gradients: the 12 cube edge directions (Perlin 2002), padded to 16 entries.
inline float grad3(std::uint8_t hash, float x, float y, float z) {
    switch (hash & 15u) {
    case 0: return x + y;
    case 1: return -x + y;
    case 2: return x - y;
    case 3: return -x - y;
    case 4: return x + z;
    case 5: return -x + z;
    case 6: return x - z;
    case 7: return -x - z;
    case 8: return y + z;
    case 9: return -y + z;
    case 10: return y - z;
    case 11: return -y - z;
    case 12: return x + y;
    case 13: return -y + z;
    case 14: return -x + y;
    default: return -y - z;
    }
}

// Per-octave coordinate offsets so lattice points of different octaves never line up.
constexpr float kOctaveOffset = 37.719f;

} // namespace

PerlinNoise::PerlinNoise(std::uint64_t seed) {
    std::array<std::uint8_t, 256> p{};
    std::iota(p.begin(), p.end(), std::uint8_t{0});
    SplitMix64 rng(seed ^ 0x5DEECE66Dull);
    for (std::uint32_t i = 255; i > 0; --i) { // Fisher-Yates shuffle
        const std::uint32_t j = rng.nextBelow(i + 1);
        std::swap(p[i], p[j]);
    }
    for (std::size_t i = 0; i < 512; ++i) {
        m_perm[i] = p[i & 255];
    }
}

float PerlinNoise::noise2(float x, float y) const {
    const int xi = fastFloor(x);
    const int yi = fastFloor(y);
    const float xf = x - static_cast<float>(xi);
    const float yf = y - static_cast<float>(yi);
    const int X = xi & 255;
    const int Y = yi & 255;
    const float u = fade(xf);
    const float v = fade(yf);

    const auto& P = m_perm;
    const int a = P[static_cast<std::size_t>(X)] + Y;
    const int b = P[static_cast<std::size_t>(X + 1)] + Y;
    const float n00 = grad2(P[static_cast<std::size_t>(a)], xf, yf);
    const float n10 = grad2(P[static_cast<std::size_t>(b)], xf - 1.0f, yf);
    const float n01 = grad2(P[static_cast<std::size_t>(a + 1)], xf, yf - 1.0f);
    const float n11 = grad2(P[static_cast<std::size_t>(b + 1)], xf - 1.0f, yf - 1.0f);

    // Unit gradients give a range of +-sqrt(1/2); rescale to +-1.
    const float n = lerpf(lerpf(n00, n10, u), lerpf(n01, n11, u), v) * 1.41421356f;
    return clampf(n, -1.0f, 1.0f);
}

float PerlinNoise::noise3(float x, float y, float z) const {
    const int xi = fastFloor(x);
    const int yi = fastFloor(y);
    const int zi = fastFloor(z);
    const float xf = x - static_cast<float>(xi);
    const float yf = y - static_cast<float>(yi);
    const float zf = z - static_cast<float>(zi);
    const int X = xi & 255;
    const int Y = yi & 255;
    const int Z = zi & 255;
    const float u = fade(xf);
    const float v = fade(yf);
    const float w = fade(zf);

    const auto& P = m_perm;
    auto at = [&P](int i) { return P[static_cast<std::size_t>(i)]; };
    const int A = at(X) + Y;
    const int AA = at(A) + Z;
    const int AB = at(A + 1) + Z;
    const int B = at(X + 1) + Y;
    const int BA = at(B) + Z;
    const int BB = at(B + 1) + Z;

    const float x0 = lerpf(grad3(at(AA), xf, yf, zf), grad3(at(BA), xf - 1, yf, zf), u);
    const float x1 = lerpf(grad3(at(AB), xf, yf - 1, zf), grad3(at(BB), xf - 1, yf - 1, zf), u);
    const float x2 = lerpf(grad3(at(AA + 1), xf, yf, zf - 1), grad3(at(BA + 1), xf - 1, yf, zf - 1), u);
    const float x3 = lerpf(grad3(at(AB + 1), xf, yf - 1, zf - 1), grad3(at(BB + 1), xf - 1, yf - 1, zf - 1), u);
    const float n = lerpf(lerpf(x0, x1, v), lerpf(x2, x3, v), w);
    return clampf(n, -1.0f, 1.0f);
}

float PerlinNoise::fbm2(float x, float y, const FbmParams& params) const {
    float sum = 0.0f;
    float amplitude = 1.0f;
    float norm = 0.0f;
    float frequency = params.frequency;
    for (int o = 0; o < params.octaves; ++o) {
        const float offset = static_cast<float>(o) * kOctaveOffset;
        sum += amplitude * noise2(x * frequency + offset, y * frequency - offset);
        norm += amplitude;
        amplitude *= params.gain;
        frequency *= params.lacunarity;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

float PerlinNoise::fbm3(float x, float y, float z, const FbmParams& params) const {
    float sum = 0.0f;
    float amplitude = 1.0f;
    float norm = 0.0f;
    float frequency = params.frequency;
    for (int o = 0; o < params.octaves; ++o) {
        const float offset = static_cast<float>(o) * kOctaveOffset;
        sum += amplitude * noise3(x * frequency + offset, y * frequency + offset * 0.5f, z * frequency - offset);
        norm += amplitude;
        amplitude *= params.gain;
        frequency *= params.lacunarity;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

float PerlinNoise::ridged2(float x, float y, const FbmParams& params) const {
    float sum = 0.0f;
    float amplitude = 1.0f;
    float norm = 0.0f;
    float frequency = params.frequency;
    for (int o = 0; o < params.octaves; ++o) {
        const float offset = static_cast<float>(o) * kOctaveOffset;
        const float r = 1.0f - std::fabs(noise2(x * frequency + offset, y * frequency - offset));
        sum += amplitude * r * r;
        norm += amplitude;
        amplitude *= params.gain;
        frequency *= params.lacunarity;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

} // namespace voxelforge
