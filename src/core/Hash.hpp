#pragma once

// Deterministic hashing and pseudo random helpers. Everything here is a pure function of
// its inputs, which is what makes world generation reproducible from a seed and safe to
// run on any thread.

#include <cstddef>
#include <cstdint>
#include <span>

namespace voxelforge {

// SplitMix64 finaliser: excellent bit mixing for a single 64-bit value.
inline constexpr std::uint64_t mix64(std::uint64_t z) {
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

inline constexpr std::uint64_t hashCoords(std::uint64_t seed, int x, int z) {
    std::uint64_t h = mix64(seed ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) * 0x9E3779B1ull));
    return mix64(h ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(z)) * 0x85EBCA77ull));
}

inline constexpr std::uint64_t hashCoords(std::uint64_t seed, int x, int y, int z) {
    return mix64(hashCoords(seed, x, z) ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) * 0xC2B2AE3Dull));
}

// Uniform float in [0, 1) from the top 24 bits of a hash.
inline constexpr float hashToUnitFloat(std::uint64_t h) {
    return static_cast<float>(h >> 40) * (1.0f / 16777216.0f);
}

// Small, fast deterministic RNG (SplitMix64 sequence).
class SplitMix64 {
public:
    explicit constexpr SplitMix64(std::uint64_t seed) : m_state(seed) {}

    constexpr std::uint64_t next() {
        m_state += 0x9E3779B97F4A7C15ull;
        std::uint64_t z = m_state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    // Uniform integer in [0, bound).
    constexpr std::uint32_t nextBelow(std::uint32_t bound) {
        return static_cast<std::uint32_t>((next() >> 32) * bound >> 32);
    }
    constexpr float nextFloat() { return hashToUnitFloat(next()); }

private:
    std::uint64_t m_state;
};

// FNV-1a, used for content hashes (tests) and save file checksums.
inline constexpr std::uint64_t fnv1a64(std::span<const std::uint8_t> data,
                                       std::uint64_t hash = 0xCBF29CE484222325ull) {
    for (const std::uint8_t byte : data) {
        hash ^= byte;
        hash *= 0x100000001B3ull;
    }
    return hash;
}

} // namespace voxelforge
