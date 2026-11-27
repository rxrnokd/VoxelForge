#include "doctest.h"

#include "world/TerrainGenerator.hpp"

#include <memory>
#include <string>

using namespace voxelforge;

namespace {
std::uint64_t chunkHash(const TerrainGenerator& gen, ChunkPos pos) {
    auto chunk = std::make_unique<Chunk>();
    gen.generate(pos, *chunk);
    return chunk->contentHash();
}
} // namespace

TEST_CASE("terrain: same seed reproduces identical chunks") {
    const TerrainGenerator a(20241127);
    const TerrainGenerator b(20241127);
    for (int cz = -3; cz <= 3; cz += 2) {
        for (int cx = -3; cx <= 3; cx += 2) {
            CHECK(chunkHash(a, {cx, cz}) == chunkHash(b, {cx, cz}));
        }
    }
}

TEST_CASE("terrain: generation order does not matter") {
    const TerrainGenerator gen(77);
    const std::uint64_t first = chunkHash(gen, {5, -9});
    for (int i = -2; i <= 2; ++i) {
        (void)chunkHash(gen, {i, i * 3});
    }
    CHECK(chunkHash(gen, {5, -9}) == first);
}

TEST_CASE("terrain: different seeds produce different worlds") {
    const TerrainGenerator a(1);
    const TerrainGenerator b(2);
    int differing = 0;
    for (int cx = 0; cx < 6; ++cx) {
        differing += chunkHash(a, {cx, 0}) != chunkHash(b, {cx, 0}) ? 1 : 0;
    }
    CHECK(differing >= 5);
}

TEST_CASE("terrain: structural invariants") {
    const TerrainGenerator gen(4242);
    auto chunk = std::make_unique<Chunk>();
    for (int cx = -4; cx < 4; ++cx) {
        gen.generate({cx, cx * 2}, *chunk);
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                CHECK(chunk->get(x, 0, z) == BlockType::Bedrock);
                CHECK(chunk->get(x, kChunkHeight - 1, z) == BlockType::Air);
                // Nothing but air or water directly above the surface up to sea level.
                const int h = gen.surfaceHeight(ChunkPos{cx, cx * 2}.originX() + x, ChunkPos{cx, cx * 2}.originZ() + z);
                CHECK(h > 0);
                CHECK(h < kChunkHeight - 1);
            }
        }
    }
}

TEST_CASE("terrain: oceans are filled with water up to sea level") {
    const TerrainGenerator gen(4242);
    // Find a column below sea level and check that it is flooded.
    bool foundOcean = false;
    for (int wx = -2000; wx < 2000 && !foundOcean; wx += 7) {
        const int h = gen.surfaceHeight(wx, 0);
        if (h < kSeaLevel - 2) {
            const ChunkPos pos = ChunkPos::fromWorld(wx, 0);
            auto chunk = std::make_unique<Chunk>();
            gen.generate(pos, *chunk);
            const int lx = wx - pos.originX();
            CHECK(chunk->get(lx, kSeaLevel, 0) == BlockType::Water);
            CHECK(chunk->get(lx, kSeaLevel + 1, 0) == BlockType::Air);
            foundOcean = true;
        }
    }
    CHECK(foundOcean);
}

// ---------------------------------------------------------------------------------------
// Biomes, caves, ores and trees
// ---------------------------------------------------------------------------------------

#include "world/Biome.hpp"

#include <array>
#include <cmath>
#include <set>
#include <utility>

namespace {

// Finds a column deep inside a region of `biome` (scanning a coarse grid).
bool findBiome(const TerrainGenerator& gen, Biome biome, int& outX, int& outZ, int minHeight = 0) {
    auto inside = [&](int x, int z) {
        for (const auto& [dx, dz] : {std::pair{0, 0}, {64, 0}, {-64, 0}, {0, 64}, {0, -64}}) {
            const ColumnInfo c = gen.column(x + dx, z + dz);
            if (c.dominant != biome || c.height <= minHeight) return false;
        }
        return true;
    };
    for (int r = 0; r < 8000; r += 48) {
        for (int x = -r; x <= r; x += 48) {
            for (int z : {-r, r}) {
                if (inside(x, z)) {
                    outX = x;
                    outZ = z;
                    return true;
                }
            }
        }
    }
    return false;
}

int countBlocks(const Chunk& c, BlockType type) {
    int n = 0;
    for (std::size_t i = 0; i < c.storage().size(); ++i) n += c.storage()[i] == type ? 1 : 0;
    return n;
}

} // namespace

TEST_CASE("biomes: weights are normalised and continuous") {
    for (float t = -1.0f; t <= 1.0f; t += 0.1f) {
        for (float h = -1.0f; h <= 1.0f; h += 0.1f) {
            const BiomeWeights a = biomeWeights(t, h);
            const BiomeWeights b = biomeWeights(t + 0.01f, h);
            float sum = 0.0f;
            for (int i = 0; i < kBiomeCount; ++i) {
                sum += a.weight[static_cast<std::size_t>(i)];
                CHECK(std::fabs(a.weight[static_cast<std::size_t>(i)] - b.weight[static_cast<std::size_t>(i)]) < 0.08f);
            }
            CHECK(sum == doctest::Approx(1.0f));
        }
    }
    CHECK(biomeWeights(0.62f, -0.55f).dominant == Biome::Desert);
    CHECK(biomeWeights(-0.6f, 0.0f).dominant == Biome::SnowyMountains);
    CHECK(biomeWeights(0.05f, 0.6f).dominant == Biome::Forest);
    CHECK(biomeWeights(0.1f, -0.1f).dominant == Biome::Plains);
    CHECK(std::string(biomeName(Biome::Forest)) == "Forest");
}

TEST_CASE("biomes: all four biomes appear in the world") {
    const TerrainGenerator gen(20241127);
    std::set<int> seen;
    for (int x = -6000; x <= 6000; x += 97) {
        for (int z = -6000; z <= 6000; z += 97) {
            seen.insert(static_cast<int>(gen.biomeAt(x, z)));
        }
    }
    CHECK(seen.size() == static_cast<std::size_t>(kBiomeCount));
}

TEST_CASE("terrain: neighbouring columns differ smoothly (blended borders)") {
    const TerrainGenerator gen(555);
    double total = 0.0;
    int samples = 0;
    int prev = gen.surfaceHeight(-3000, 17);
    for (int x = -2999; x < 3000; ++x) {
        const int h = gen.surfaceHeight(x, 17);
        total += std::abs(h - prev);
        ++samples;
        prev = h;
    }
    CHECK(total / samples < 1.5);
}

TEST_CASE("terrain: ores only appear inside their depth ranges") {
    const TerrainGenerator gen(8080);
    int ruleCount = 0;
    const auto* rules = TerrainGenerator::oreRules(ruleCount);
    std::array<int, 4> found{};
    auto chunk = std::make_unique<Chunk>();
    for (int cx = 0; cx < 6; ++cx) {
        gen.generate({cx, -cx}, *chunk);
        for (int y = 0; y < kChunkHeight; ++y)
            for (int z = 0; z < kChunkSize; ++z)
                for (int x = 0; x < kChunkSize; ++x) {
                    const BlockType b = chunk->get(x, y, z);
                    for (int r = 0; r < ruleCount; ++r) {
                        if (b == rules[r].ore) {
                            CHECK(y >= rules[r].minY);
                            CHECK(y <= rules[r].maxY + 1);
                            ++found[static_cast<std::size_t>(r)];
                        }
                    }
                }
    }
    CHECK(found[2] > 0); // iron
    CHECK(found[3] > 0); // coal
    CHECK(found[3] > found[0]); // coal is far more common than diamond
}

TEST_CASE("terrain: caves hollow out part of the underground") {
    const TerrainGenerator gen(4242);
    auto chunk = std::make_unique<Chunk>();
    long long air = 0;
    long long total = 0;
    for (int cz = -3; cz <= 3; ++cz)
        for (int cx = -3; cx <= 3; ++cx) {
            const ChunkPos pos{cx, cz};
            gen.generate(pos, *chunk);
            for (int z = 0; z < kChunkSize; ++z)
                for (int x = 0; x < kChunkSize; ++x) {
                    const int h = gen.surfaceHeight(pos.originX() + x, pos.originZ() + z);
                    for (int y = 6; y < h - 8; ++y) {
                        air += chunk->get(x, y, z) == BlockType::Air ? 1 : 0;
                        ++total;
                    }
                }
        }
    const double fraction = static_cast<double>(air) / static_cast<double>(total);
    CHECK(fraction > 0.005);
    CHECK(fraction < 0.25);
}

TEST_CASE("terrain: forests have trees, deserts have cacti and sand, snowy biomes snow") {
    const TerrainGenerator gen(777);
    int x = 0, z = 0;
    auto chunk = std::make_unique<Chunk>();

    REQUIRE(findBiome(gen, Biome::Forest, x, z, kSeaLevel + 3));
    int wood = 0, leaves = 0;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            gen.generate({ChunkPos::fromWorld(x, z).x + dx, ChunkPos::fromWorld(x, z).z + dz}, *chunk);
            wood += countBlocks(*chunk, BlockType::Wood);
            leaves += countBlocks(*chunk, BlockType::Leaves);
        }
    CHECK(wood > 10);
    CHECK(leaves > wood);

    REQUIRE(findBiome(gen, Biome::Desert, x, z, kSeaLevel + 3));
    int cactus = 0, sand = 0;
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = -2; dx <= 2; ++dx) {
            gen.generate({ChunkPos::fromWorld(x, z).x + dx, ChunkPos::fromWorld(x, z).z + dz}, *chunk);
            cactus += countBlocks(*chunk, BlockType::Cactus);
            sand += countBlocks(*chunk, BlockType::Sand);
        }
    CHECK(cactus > 0);
    CHECK(sand > 1000);

    REQUIRE(findBiome(gen, Biome::SnowyMountains, x, z, kSeaLevel + 3));
    gen.generate(ChunkPos::fromWorld(x, z), *chunk);
    CHECK(countBlocks(*chunk, BlockType::Snow) > 50);
}

TEST_CASE("terrain: trees that cross a chunk border continue in the neighbour") {
    const TerrainGenerator gen(1234567);
    int x = 0, z = 0;
    REQUIRE(findBiome(gen, Biome::Forest, x, z, kSeaLevel + 3));
    const ChunkPos base = ChunkPos::fromWorld(x, z);
    auto a = std::make_unique<Chunk>();
    auto b = std::make_unique<Chunk>();
    int checked = 0;
    for (int i = -4; i < 16 && checked < 3; ++i) {
        const ChunkPos left{base.x + i % 4, base.z + i / 4};
        const ChunkPos right{left.x + 1, left.z};
        gen.generate(left, *a);
        gen.generate(right, *b);
        for (int lz = 1; lz < kChunkSize - 1; ++lz) {
            // Trunk in the last column of the left chunk: find its top.
            int top = -1;
            for (int y = kChunkHeight - 1; y > 0; --y) {
                if (a->get(kChunkSize - 1, y, lz) == BlockType::Wood) {
                    top = y;
                    break;
                }
            }
            if (top < 0 || a->get(kChunkSize - 1, top - 1, lz) != BlockType::Wood) {
                continue; // not a trunk
            }
            // The widest canopy layer reaches 2 blocks into the right chunk.
            const BlockType across = b->get(0, top - 1, lz);
            CHECK((across == BlockType::Leaves || across == BlockType::Wood));
            ++checked;
        }
    }
    CHECK(checked > 0);
}
