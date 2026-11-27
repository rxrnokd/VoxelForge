#include "world/Biome.hpp"

#include <cmath>

namespace voxelforge {
namespace {

struct ClimatePoint {
    float temperature;
    float humidity;
};

// Climate space position of each biome (same order as the enum).
constexpr std::array<ClimatePoint, kBiomeCount> kClimate{{
    {0.10f, -0.10f}, // Plains: temperate, moderately dry
    {0.62f, -0.55f}, // Desert: hot and dry
    {-0.60f, 0.05f}, // Snowy mountains: cold
    {0.05f, 0.55f},  // Forest: temperate and humid
}};

constexpr std::array<BiomeTerrain, kBiomeCount> kTerrain{{
    {68.0f, 6.0f, 0.0f},   // Plains
    {67.0f, 5.0f, 0.0f},   // Desert (dunes)
    {82.0f, 14.0f, 78.0f}, // Snowy mountains
    {70.0f, 11.0f, 0.0f},  // Forest
}};

constexpr float kBlendSigma = 0.30f; // width of the transition zone in climate space

} // namespace

const char* biomeName(Biome biome) {
    switch (biome) {
    case Biome::Plains: return "Plains";
    case Biome::Desert: return "Desert";
    case Biome::SnowyMountains: return "Snowy Mountains";
    case Biome::Forest: return "Forest";
    case Biome::Count: break;
    }
    return "Unknown";
}

const BiomeTerrain& biomeTerrain(Biome biome) { return kTerrain[static_cast<std::size_t>(biome)]; }

BiomeWeights biomeWeights(float temperature, float humidity) {
    BiomeWeights w;
    float total = 0.0f;
    float best = -1.0f;
    for (int i = 0; i < kBiomeCount; ++i) {
        const float dt = temperature - kClimate[static_cast<std::size_t>(i)].temperature;
        const float dh = humidity - kClimate[static_cast<std::size_t>(i)].humidity;
        const float value = std::exp(-(dt * dt + dh * dh) / (kBlendSigma * kBlendSigma));
        w.weight[static_cast<std::size_t>(i)] = value;
        total += value;
        if (value > best) {
            best = value;
            w.dominant = static_cast<Biome>(i);
        }
    }
    if (total <= 1e-20f) {
        // Extremely far from every biome: fall back to the dominant one only.
        w.weight.fill(0.0f);
        w.weight[static_cast<std::size_t>(w.dominant)] = 1.0f;
        return w;
    }
    for (float& v : w.weight) {
        v /= total;
    }
    return w;
}

Biome sampleBiome(const BiomeWeights& weights, float random01) {
    // Sharpen the weights (fourth power) so dithering stays in a narrow band along the
    // borders instead of sprinkling foreign blocks deep inside a biome.
    std::array<float, kBiomeCount> sharp{};
    float total = 0.0f;
    for (int i = 0; i < kBiomeCount; ++i) {
        const float w = weights.weight[static_cast<std::size_t>(i)];
        sharp[static_cast<std::size_t>(i)] = (w * w) * (w * w);
        total += sharp[static_cast<std::size_t>(i)];
    }
    if (total <= 0.0f) {
        return weights.dominant;
    }
    float accumulated = 0.0f;
    for (int i = 0; i < kBiomeCount; ++i) {
        accumulated += sharp[static_cast<std::size_t>(i)] / total;
        if (random01 < accumulated) {
            return static_cast<Biome>(i);
        }
    }
    return weights.dominant;
}

} // namespace voxelforge
