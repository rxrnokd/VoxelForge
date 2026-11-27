#pragma once

#include <array>
#include <cstdint>

namespace voxelforge {

enum class Biome : std::uint8_t { Plains = 0, Desert, SnowyMountains, Forest, Count };
inline constexpr int kBiomeCount = static_cast<int>(Biome::Count);

const char* biomeName(Biome biome);

// Shape of the terrain in a biome. Heights of neighbouring biomes are blended with the
// climate weights, so borders turn into smooth slopes instead of cliffs.
struct BiomeTerrain {
    float baseHeight;        // average surface height
    float hillAmplitude;     // scale of the rolling fBm hills
    float mountainAmplitude; // scale of the ridged mountain noise
};
const BiomeTerrain& biomeTerrain(Biome biome);

// Blend weights of every biome for a climate sample (temperature and humidity in [-1, 1]).
// Each biome owns a point in climate space; weights fall off with a Gaussian of the
// distance to it and are normalised to sum to one.
struct BiomeWeights {
    std::array<float, kBiomeCount> weight{};
    Biome dominant = Biome::Plains;
};
BiomeWeights biomeWeights(float temperature, float humidity);

// Picks a biome at random in proportion to its (sharpened) weight, using `random01` in
// [0, 1). Feeding a per-column hash dithers surface blocks along biome borders.
Biome sampleBiome(const BiomeWeights& weights, float random01);

} // namespace voxelforge
