#include "world/TerrainGenerator.hpp"

#include "core/Hash.hpp"
#include "core/Math.hpp"

#include <algorithm>
#include <array>

namespace voxelforge {
namespace {

constexpr FbmParams kClimate{3, 1.0f / 520.0f, 2.0f, 0.5f};
constexpr FbmParams kContinental{4, 1.0f / 640.0f, 2.0f, 0.5f};
constexpr FbmParams kHills{5, 1.0f / 110.0f, 2.0f, 0.5f};
constexpr FbmParams kRidges{4, 1.0f / 300.0f, 2.0f, 0.5f};
constexpr FbmParams kTunnels{2, 1.0f / 56.0f, 2.0f, 0.45f};
constexpr FbmParams kCaverns{2, 1.0f / 80.0f, 2.0f, 0.5f};

constexpr int kSnowLine = 118;      // any biome gets snow caps above this height
constexpr int kOceanFloor = 46;
constexpr float kTunnelRadiusSq = 0.0020f;
constexpr float kCavernThreshold = 0.63f;
constexpr int kCavernMaxY = 52;
constexpr int kMinCaveY = 5;

// Cave noise is sampled on a coarse lattice and trilinearly interpolated (the classic
// trick that makes 3D noise affordable for every block of a chunk).
constexpr int kCaveCellXZ = 4;
constexpr int kCaveCellY = 8;
constexpr int kCaveSamplesXZ = kChunkSize / kCaveCellXZ + 1;
constexpr int kCaveSamplesY = kChunkHeight / kCaveCellY + 1;

// Salts that decorrelate the different hash based decisions.
constexpr std::uint64_t kSaltBiomeDither = 0xB10E;
constexpr std::uint64_t kSaltBedrock = 0xBED0;
constexpr std::uint64_t kSaltGravel = 0x6AE1;
constexpr std::uint64_t kSaltOreCell = 0x0E5C;
constexpr std::uint64_t kSaltOreBlock = 0x0E5B;
constexpr std::uint64_t kSaltTree = 0x7EE5;

constexpr int kTreeCell = 5;   // one tree candidate per 5x5 cell
constexpr int kTreeMargin = 3; // widest canopy radius + 1

constexpr TerrainGenerator::OreRule kOreRules[] = {
    // Rarest first: a cell becomes the first ore whose cumulative chance it falls under.
    {BlockType::DiamondOre, kMinCaveY, 16, 0.0035f},
    {BlockType::GoldOre, kMinCaveY, 32, 0.0060f},
    {BlockType::IronOre, kMinCaveY, 64, 0.0140f},
    {BlockType::CoalOre, kMinCaveY, 128, 0.0220f},
};

float climate(const PerlinNoise& noise, float x, float z) {
    return clampf(noise.fbm2(x, z, kClimate) * 1.7f, -1.0f, 1.0f);
}

enum class Replace { OnlyAir, AirOrLeaves };

void setIfInside(Chunk& out, ChunkPos pos, int wx, int y, int wz, BlockType block, Replace mode) {
    const int lx = wx - pos.originX();
    const int lz = wz - pos.originZ();
    if (!Chunk::inBounds(lx, y, lz)) {
        return;
    }
    const BlockType current = out.get(lx, y, lz);
    if (current == BlockType::Air || (mode == Replace::AirOrLeaves && current == BlockType::Leaves)) {
        out.set(lx, y, lz, block);
    }
}

void growOak(Chunk& out, ChunkPos pos, int x, int base, int z, std::uint64_t h) {
    const int trunk = 4 + static_cast<int>((h >> 16) % 3);
    const int top = base + trunk - 1;
    for (int dy = -2; dy <= 1; ++dy) {
        const int radius = dy <= -1 ? 2 : 1;
        for (int dz = -radius; dz <= radius; ++dz) {
            for (int dx = -radius; dx <= radius; ++dx) {
                const bool corner = std::abs(dx) == radius && std::abs(dz) == radius;
                if (corner && (dy == 1 || radius == 1 ||
                               ((h >> (20 + ((dx + 2) * 5 + (dz + 2)) % 40)) & 1u) == 0)) {
                    continue; // rounded canopy with a few random gaps on the corners
                }
                setIfInside(out, pos, x + dx, top + dy, z + dz, BlockType::Leaves, Replace::OnlyAir);
            }
        }
    }
    for (int y = base; y <= top; ++y) {
        setIfInside(out, pos, x, y, z, BlockType::Wood, Replace::AirOrLeaves);
    }
}

void growSpruce(Chunk& out, ChunkPos pos, int x, int base, int z, std::uint64_t h) {
    const int trunk = 6 + static_cast<int>((h >> 16) % 3);
    const int top = base + trunk - 1;
    for (int y = base + 2; y <= top + 1; ++y) {
        int radius = ((top - y) % 2 == 0) ? 1 : 2;
        if (y >= top) {
            radius = y == top ? 1 : 0;
        }
        for (int dz = -radius; dz <= radius; ++dz) {
            for (int dx = -radius; dx <= radius; ++dx) {
                if (radius == 2 && std::abs(dx) == 2 && std::abs(dz) == 2) {
                    continue;
                }
                if (radius == 1 && y == top && std::abs(dx) == 1 && std::abs(dz) == 1) {
                    continue;
                }
                setIfInside(out, pos, x + dx, y, z + dz, BlockType::Leaves, Replace::OnlyAir);
            }
        }
    }
    for (int y = base; y <= top; ++y) {
        setIfInside(out, pos, x, y, z, BlockType::Wood, Replace::AirOrLeaves);
    }
}

void growCactus(Chunk& out, ChunkPos pos, int x, int base, int z, std::uint64_t h) {
    const int height = 1 + static_cast<int>((h >> 16) % 3);
    for (int y = base; y < base + height; ++y) {
        setIfInside(out, pos, x, y, z, BlockType::Cactus, Replace::OnlyAir);
    }
}

} // namespace

TerrainGenerator::TerrainGenerator(std::uint64_t seed)
    : m_seed(seed),
      m_temperature(mix64(seed ^ 0x1001)),
      m_humidity(mix64(seed ^ 0x1002)),
      m_continental(mix64(seed ^ 0x1003)),
      m_hills(mix64(seed ^ 0x1004)),
      m_mountains(mix64(seed ^ 0x1005)),
      m_caveA(mix64(seed ^ 0x1006)),
      m_caveB(mix64(seed ^ 0x1007)),
      m_cavern(mix64(seed ^ 0x1008)),
      m_entrances(mix64(seed ^ 0x1009)) {}

const TerrainGenerator::OreRule* TerrainGenerator::oreRules(int& count) {
    count = static_cast<int>(std::size(kOreRules));
    return kOreRules;
}

ColumnInfo TerrainGenerator::column(int wx, int wz) const {
    const float x = static_cast<float>(wx);
    const float z = static_cast<float>(wz);

    const BiomeWeights weights = biomeWeights(climate(m_temperature, x, z), climate(m_humidity, x, z));
    const float hills = m_hills.fbm2(x, z, kHills);
    const float ridge = m_mountains.ridged2(x, z, kRidges);
    const float ridgeShape = ridge * ridge * ridge; // sharpen: wide valleys, narrow peaks

    float height = 0.0f;
    for (int b = 0; b < kBiomeCount; ++b) {
        const BiomeTerrain& t = biomeTerrain(static_cast<Biome>(b));
        height += weights.weight[static_cast<std::size_t>(b)] *
                  (t.baseHeight + hills * t.hillAmplitude + ridgeShape * t.mountainAmplitude);
    }

    // Continental noise pulls large regions down below sea level to form oceans.
    const float continental = m_continental.fbm2(x, z, kContinental);
    const float ocean = smoothstep(0.10f, 0.30f, -continental);
    height = lerpf(height, static_cast<float>(kOceanFloor) + hills * 5.0f, ocean);

    ColumnInfo info;
    info.height = std::clamp(fastFloor(height), 2, kChunkHeight - 12);
    info.dominant = weights.dominant;
    info.biome = sampleBiome(weights, hashToUnitFloat(hashCoords(m_seed ^ kSaltBiomeDither, wx, wz)));
    info.caveEntrance = m_entrances.noise2(x / 72.0f, z / 72.0f) > 0.55f;
    return info;
}

void TerrainGenerator::generate(ChunkPos pos, Chunk& out) const {
    std::array<ColumnInfo, kChunkArea> columns;
    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int lx = 0; lx < kChunkSize; ++lx) {
            columns[static_cast<std::size_t>(lz * kChunkSize + lx)] = column(pos.originX() + lx, pos.originZ() + lz);
        }
    }
    out.fill(BlockType::Air);
    fillColumns(pos, out, columns.data());
    carveCaves(pos, out, columns.data());
    placeOres(pos, out);
    placeTrees(pos, out);
}

void TerrainGenerator::fillColumns(ChunkPos pos, Chunk& out, const ColumnInfo* columns) const {
    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int lx = 0; lx < kChunkSize; ++lx) {
            const ColumnInfo& col = columns[lz * kChunkSize + lx];
            const int wx = pos.originX() + lx;
            const int wz = pos.originZ() + lz;
            const int h = col.height;

            BlockType top = BlockType::Grass;
            BlockType filler = BlockType::Dirt;
            int fillerDepth = 3;
            if (h < kSeaLevel - 1) {
                const bool gravel = h < kSeaLevel - 6 &&
                                    hashToUnitFloat(hashCoords(m_seed ^ kSaltGravel, wx, wz)) < 0.35f;
                top = gravel ? BlockType::Gravel : BlockType::Sand;
                filler = BlockType::Sand;
            } else if (h <= kSeaLevel + 1 && col.biome != Biome::SnowyMountains) {
                top = BlockType::Sand; // beaches
                filler = BlockType::Sand;
            } else {
                switch (col.biome) {
                case Biome::Desert:
                    top = BlockType::Sand;
                    filler = BlockType::Sand;
                    fillerDepth = 4;
                    break;
                case Biome::SnowyMountains:
                    top = BlockType::Snow;
                    filler = h > 100 ? BlockType::Stone : BlockType::Dirt;
                    break;
                case Biome::Plains:
                case Biome::Forest:
                case Biome::Count:
                    break;
                }
                if (h >= kSnowLine) {
                    top = BlockType::Snow;
                    filler = BlockType::Stone;
                }
            }

            out.set(lx, 0, lz, BlockType::Bedrock);
            for (int y = 1; y <= 3; ++y) {
                const float chance = 0.75f - 0.25f * static_cast<float>(y - 1);
                if (hashToUnitFloat(hashCoords(m_seed ^ kSaltBedrock, wx, y, wz)) < chance) {
                    out.set(lx, y, lz, BlockType::Bedrock);
                } else {
                    out.set(lx, y, lz, BlockType::Stone);
                }
            }
            for (int y = 4; y <= h; ++y) {
                BlockType block = BlockType::Stone;
                if (y == h) {
                    block = top;
                } else if (y > h - 1 - fillerDepth) {
                    block = filler;
                }
                out.set(lx, y, lz, block);
            }
            for (int y = h + 1; y <= kSeaLevel; ++y) {
                out.set(lx, y, lz, BlockType::Water);
            }
        }
    }
}

void TerrainGenerator::carveCaves(ChunkPos pos, Chunk& out, const ColumnInfo* columns) const {
    int maxHeight = 0;
    for (int i = 0; i < kChunkArea; ++i) {
        maxHeight = std::max(maxHeight, columns[i].height);
    }
    const int samplesY = std::min(kCaveSamplesY, maxHeight / kCaveCellY + 2);

    // Coarse lattice of the three cave fields.
    constexpr int kFields = 3;
    std::array<float, kFields * kCaveSamplesXZ * kCaveSamplesY * kCaveSamplesXZ> lattice{};
    auto latticeIndex = [](int f, int i, int j, int k) {
        return static_cast<std::size_t>(((f * kCaveSamplesY + j) * kCaveSamplesXZ + k) * kCaveSamplesXZ + i);
    };
    for (int j = 0; j < samplesY; ++j) {
        const float y = static_cast<float>(j * kCaveCellY);
        for (int k = 0; k < kCaveSamplesXZ; ++k) {
            const float z = static_cast<float>(pos.originZ() + k * kCaveCellXZ);
            for (int i = 0; i < kCaveSamplesXZ; ++i) {
                const float x = static_cast<float>(pos.originX() + i * kCaveCellXZ);
                // Squash the vertical axis so tunnels run mostly horizontally.
                lattice[latticeIndex(0, i, j, k)] = m_caveA.fbm3(x, y * 1.7f, z, kTunnels);
                lattice[latticeIndex(1, i, j, k)] = m_caveB.fbm3(x, y * 1.7f, z, kTunnels);
                lattice[latticeIndex(2, i, j, k)] = m_cavern.fbm3(x, y * 2.0f, z, kCaverns);
            }
        }
    }

    for (int lz = 0; lz < kChunkSize; ++lz) {
        const int k = lz / kCaveCellXZ;
        const float fz = static_cast<float>(lz % kCaveCellXZ) / kCaveCellXZ;
        for (int lx = 0; lx < kChunkSize; ++lx) {
            const int i = lx / kCaveCellXZ;
            const float fx = static_cast<float>(lx % kCaveCellXZ) / kCaveCellXZ;
            const ColumnInfo& col = columns[lz * kChunkSize + lx];
            int top = col.caveEntrance ? col.height : col.height - 4;
            if (col.height < kSeaLevel + 2) {
                top = col.height - 8; // keep a thick floor under the sea
            }
            for (int y = kMinCaveY; y <= top; ++y) {
                const int j = y / kCaveCellY;
                const float fy = static_cast<float>(y % kCaveCellY) / kCaveCellY;
                float values[kFields];
                for (int f = 0; f < kFields; ++f) {
                    const float c00 = lerpf(lattice[latticeIndex(f, i, j, k)], lattice[latticeIndex(f, i + 1, j, k)], fx);
                    const float c10 = lerpf(lattice[latticeIndex(f, i, j + 1, k)], lattice[latticeIndex(f, i + 1, j + 1, k)], fx);
                    const float c01 = lerpf(lattice[latticeIndex(f, i, j, k + 1)], lattice[latticeIndex(f, i + 1, j, k + 1)], fx);
                    const float c11 = lerpf(lattice[latticeIndex(f, i, j + 1, k + 1)], lattice[latticeIndex(f, i + 1, j + 1, k + 1)], fx);
                    values[f] = lerpf(lerpf(c00, c10, fy), lerpf(c01, c11, fy), fz);
                }
                // Spaghetti tunnels follow the intersection of two noise zero-surfaces;
                // caverns are the high points of a third field, deep underground only.
                const bool tunnel = values[0] * values[0] + values[1] * values[1] < kTunnelRadiusSq;
                const bool cavern = y < kCavernMaxY && values[2] > kCavernThreshold;
                if (tunnel || cavern) {
                    const BlockType current = out.get(lx, y, lz);
                    if (current != BlockType::Bedrock && current != BlockType::Water) {
                        out.set(lx, y, lz, BlockType::Air);
                    }
                }
            }
        }
    }
}

void TerrainGenerator::placeOres(ChunkPos pos, Chunk& out) const {
    // Ores come in clusters: a 2x2x2 cell is picked by a hash, then most stone blocks
    // inside it become the ore.
    const int cellOriginX = pos.originX() / 2;
    const int cellOriginZ = pos.originZ() / 2;
    const int maxCellY = kOreRules[std::size(kOreRules) - 1].maxY / 2;
    for (int cy = kMinCaveY / 2; cy <= maxCellY; ++cy) {
        for (int cz = 0; cz < kChunkSize / 2; ++cz) {
            for (int cx = 0; cx < kChunkSize / 2; ++cx) {
                const std::uint64_t h = hashCoords(m_seed ^ kSaltOreCell, cellOriginX + cx, cy, cellOriginZ + cz);
                float roll = hashToUnitFloat(h);
                BlockType ore = BlockType::Air;
                for (const OreRule& rule : kOreRules) {
                    if (cy * 2 < rule.minY || cy * 2 > rule.maxY) {
                        continue;
                    }
                    if (roll < rule.cellChance) {
                        ore = rule.ore;
                        break;
                    }
                    roll -= rule.cellChance;
                }
                if (ore == BlockType::Air) {
                    continue;
                }
                for (int b = 0; b < 8; ++b) {
                    const int lx = cx * 2 + (b & 1);
                    const int y = cy * 2 + ((b >> 1) & 1);
                    const int lz = cz * 2 + (b >> 2);
                    if (out.get(lx, y, lz) != BlockType::Stone) {
                        continue;
                    }
                    const std::uint64_t bh =
                        hashCoords(m_seed ^ kSaltOreBlock, pos.originX() + lx, y, pos.originZ() + lz);
                    if (hashToUnitFloat(bh) < 0.7f) {
                        out.set(lx, y, lz, ore);
                    }
                }
            }
        }
    }
}

void TerrainGenerator::placeTrees(ChunkPos pos, Chunk& out) const {
    const int minX = pos.originX() - kTreeMargin;
    const int maxX = pos.originX() + kChunkSize - 1 + kTreeMargin;
    const int minZ = pos.originZ() - kTreeMargin;
    const int maxZ = pos.originZ() + kChunkSize - 1 + kTreeMargin;
    for (int cz = floorDiv(minZ, kTreeCell); cz <= floorDiv(maxZ, kTreeCell); ++cz) {
        for (int cx = floorDiv(minX, kTreeCell); cx <= floorDiv(maxX, kTreeCell); ++cx) {
            const std::uint64_t h = hashCoords(m_seed ^ kSaltTree, cx, cz);
            // Candidate position inside the cell, kept 1 block from the edges so that
            // neighbouring trees never fuse.
            const int x = cx * kTreeCell + 1 + static_cast<int>(h % 3);
            const int z = cz * kTreeCell + 1 + static_cast<int>((h >> 8) % 3);
            if (x < minX || x > maxX || z < minZ || z > maxZ) {
                continue;
            }
            const ColumnInfo col = column(x, z);
            if (col.caveEntrance || col.height <= kSeaLevel + 1 || col.height >= kChunkHeight - 16) {
                continue;
            }
            float chance = 0.0f;
            switch (col.biome) {
            case Biome::Forest: chance = 0.80f; break;
            case Biome::Plains: chance = 0.05f; break;
            case Biome::SnowyMountains: chance = col.height < 112 ? 0.25f : 0.0f; break;
            case Biome::Desert: chance = 0.12f; break;
            case Biome::Count: break;
            }
            if (hashToUnitFloat(mix64(h)) >= chance) {
                continue;
            }
            const int base = col.height + 1;
            if (col.biome == Biome::Desert) {
                growCactus(out, pos, x, base, z, h);
            } else if (col.biome == Biome::SnowyMountains) {
                growSpruce(out, pos, x, base, z, h);
            } else {
                growOak(out, pos, x, base, z, h);
            }
        }
    }
}

} // namespace voxelforge
