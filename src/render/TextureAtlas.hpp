#pragma once

#include "world/Block.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace voxelforge {

inline constexpr int kTilePixels = 16;
inline constexpr int kAtlasColumns = 16;
inline constexpr int kAtlasRows = (kTileCount + kAtlasColumns - 1) / kAtlasColumns;
inline constexpr int kAtlasWidth = kAtlasColumns * kTilePixels;
inline constexpr int kAtlasHeight = kAtlasRows * kTilePixels;
inline constexpr int kAtlasTileLevels = 5; // mip levels the shader samples: 16, 8, 4, 2, 1 px per tile

// OpenGL only treats a mipmapped texture as complete when the whole chain down to 1x1 is
// present, so the atlas stores the full chain; levels past kAtlasTileLevels are unused.
constexpr int fullMipChainLength(int w, int h) {
    int levels = 1;
    for (int size = w > h ? w : h; size > 1; size >>= 1) {
        ++levels;
    }
    return levels;
}
inline constexpr int kAtlasMipLevels = fullMipChainLength(kAtlasWidth, kAtlasHeight);

struct Rgba {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
    constexpr bool operator==(const Rgba&) const = default;
};

// RGBA8 atlas of procedurally generated 16x16 pixel-art tiles plus a per-tile mip chain.
// Tiles are 16 pixels (a power of two), so each 2x2 box-filter step shrinks every tile in
// place and the first kAtlasTileLevels levels never bleed between tiles. The shader picks
// the level manually and reads texels with texelFetch.
class TextureAtlas {
public:
    static TextureAtlas generate();

    static int width(int level = 0) { return std::max(1, kAtlasWidth >> level); }
    static int height(int level = 0) { return std::max(1, kAtlasHeight >> level); }

    // All mip levels concatenated, level 0 first (the layout raylib expects).
    const std::vector<std::uint8_t>& data() const { return m_data; }
    static std::size_t levelOffset(int level);

    Rgba pixel(int x, int y, int level = 0) const;
    Rgba tilePixel(Tile tile, int px, int py) const;

    static int tileX(Tile tile) { return (static_cast<int>(tile) % kAtlasColumns) * kTilePixels; }
    static int tileY(Tile tile) { return (static_cast<int>(tile) / kAtlasColumns) * kTilePixels; }

private:
    void setPixel(int x, int y, Rgba color);
    void buildMipChain();

    std::vector<std::uint8_t> m_data;
};

} // namespace voxelforge
