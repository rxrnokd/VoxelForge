#include "doctest.h"

#include "core/Hash.hpp"
#include "render/TextureAtlas.hpp"

#include <set>
#include <vector>

using namespace voxelforge;

TEST_CASE("atlas: dimensions and mip chain size") {
    const TextureAtlas atlas = TextureAtlas::generate();
    CHECK(atlas.width() == kAtlasColumns * kTilePixels);
    CHECK(atlas.height() == kAtlasRows * kTilePixels);
    std::size_t expected = 0;
    for (int l = 0; l < kAtlasMipLevels; ++l) {
        expected += static_cast<std::size_t>(atlas.width(l) * atlas.height(l) * 4);
    }
    CHECK(atlas.data().size() == expected);
    CHECK((kTilePixels >> (kAtlasTileLevels - 1)) == 1);
    CHECK(atlas.width(kAtlasMipLevels - 1) == 1);
    CHECK(atlas.height(kAtlasMipLevels - 1) == 1);
}

TEST_CASE("atlas: generation is deterministic") {
    const TextureAtlas a = TextureAtlas::generate();
    const TextureAtlas b = TextureAtlas::generate();
    CHECK(a.data() == b.data());
}

TEST_CASE("atlas: every tile is distinct and non-empty") {
    const TextureAtlas atlas = TextureAtlas::generate();
    std::set<std::uint64_t> unique;
    for (int t = 0; t < kTileCount; ++t) {
        std::vector<std::uint8_t> bytes;
        int opaquePixels = 0;
        for (int y = 0; y < kTilePixels; ++y)
            for (int x = 0; x < kTilePixels; ++x) {
                const Rgba p = atlas.tilePixel(static_cast<Tile>(t), x, y);
                bytes.insert(bytes.end(), {p.r, p.g, p.b, p.a});
                opaquePixels += p.a > 127 ? 1 : 0;
            }
        CHECK(opaquePixels > 0);
        unique.insert(fnv1a64(bytes));
    }
    CHECK(unique.size() == static_cast<std::size_t>(kTileCount));
}

TEST_CASE("atlas: glass is a cutout, solid blocks are opaque") {
    const TextureAtlas atlas = TextureAtlas::generate();
    CHECK(atlas.tilePixel(Tile::Glass, 8, 8).a == 0);
    CHECK(atlas.tilePixel(Tile::Glass, 0, 8).a == 255);
    for (int y = 0; y < kTilePixels; ++y)
        for (int x = 0; x < kTilePixels; ++x)
            CHECK(atlas.tilePixel(Tile::Stone, x, y).a == 255);
    // Grass side: green on top, dirt brown at the bottom.
    const Rgba top = atlas.tilePixel(Tile::GrassSide, 5, 0);
    const Rgba bottom = atlas.tilePixel(Tile::GrassSide, 5, 15);
    CHECK(top.g > top.r);
    CHECK(bottom.r > bottom.g);
}
