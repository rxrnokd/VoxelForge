#include "render/TextureAtlas.hpp"

#include "core/Hash.hpp"
#include "core/Math.hpp"

#include <array>
#include <cmath>

namespace voxelforge {
namespace {

using Image16 = std::array<Rgba, kTilePixels * kTilePixels>;

constexpr Rgba rgb(int r, int g, int b, int a = 255) {
    return {static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b),
            static_cast<std::uint8_t>(a)};
}

Rgba shade(Rgba c, float f) {
    auto ch = [f](std::uint8_t v) {
        return static_cast<std::uint8_t>(clampf(static_cast<float>(v) * f, 0.0f, 255.0f));
    };
    return {ch(c.r), ch(c.g), ch(c.b), c.a};
}

// Deterministic per-pixel randomness for one tile.
class TileRandom {
public:
    explicit TileRandom(Tile tile) : m_seed(mix64(0xA71A5ull + static_cast<std::uint64_t>(tile))) {}

    float at(int x, int y, int salt = 0) const {
        return hashToUnitFloat(hashCoords(m_seed + static_cast<std::uint64_t>(salt), x, y));
    }

    // Smooth value noise that tiles seamlessly over the 16 pixel tile.
    float smooth(int x, int y, int cell, int salt = 0) const {
        const int cells = kTilePixels / cell;
        const int cx = x / cell;
        const int cy = y / cell;
        const float fx = static_cast<float>(x % cell) / static_cast<float>(cell);
        const float fy = static_cast<float>(y % cell) / static_cast<float>(cell);
        auto corner = [&](int ix, int iy) { return at(floorMod(ix, cells), floorMod(iy, cells), salt + 991); };
        const float top = lerpf(corner(cx, cy), corner(cx + 1, cy), fx);
        const float bottom = lerpf(corner(cx, cy + 1), corner(cx + 1, cy + 1), fx);
        return lerpf(top, bottom, fy);
    }

    SplitMix64 rng() const { return SplitMix64(m_seed); }

private:
    std::uint64_t m_seed;
};

Rgba& px(Image16& img, int x, int y) { return img[static_cast<std::size_t>(y * kTilePixels + x)]; }

void noisyFill(Image16& img, const TileRandom& r, Rgba base, float jitter, float blotch = 0.0f, int cell = 4) {
    for (int y = 0; y < kTilePixels; ++y) {
        for (int x = 0; x < kTilePixels; ++x) {
            const float f = 1.0f + (r.at(x, y) - 0.5f) * 2.0f * jitter + (r.smooth(x, y, cell) - 0.5f) * 2.0f * blotch;
            px(img, x, y) = shade(base, f);
        }
    }
}

constexpr Rgba kDirt = rgb(134, 96, 67);
constexpr Rgba kGrass = rgb(98, 160, 56);
constexpr Rgba kStone = rgb(126, 126, 128);

void paintDirt(Image16& img, const TileRandom& r) {
    noisyFill(img, r, kDirt, 0.14f, 0.06f);
    for (int y = 0; y < kTilePixels; ++y) {
        for (int x = 0; x < kTilePixels; ++x) {
            const float v = r.at(x, y, 7);
            if (v < 0.10f) {
                px(img, x, y) = shade(kDirt, 0.68f);
            } else if (v > 0.95f) {
                px(img, x, y) = shade(rgb(150, 120, 95), 1.08f);
            }
        }
    }
}

void paintStone(Image16& img, const TileRandom& r) {
    noisyFill(img, r, kStone, 0.06f, 0.12f);
    for (int y = 0; y < kTilePixels; ++y) {
        for (int x = 0; x < kTilePixels; ++x) {
            if (r.at(x, y, 3) < 0.06f) {
                px(img, x, y) = shade(kStone, 0.72f);
            }
        }
    }
}

void paintGrassTop(Image16& img, const TileRandom& r) {
    noisyFill(img, r, kGrass, 0.14f, 0.08f);
    for (int y = 0; y < kTilePixels; ++y) {
        for (int x = 0; x < kTilePixels; ++x) {
            const float v = r.at(x, y, 5);
            if (v < 0.08f) {
                px(img, x, y) = shade(kGrass, 0.75f);
            } else if (v > 0.93f) {
                px(img, x, y) = shade(kGrass, 1.2f);
            }
        }
    }
}

void paintGrassSide(Image16& img, const TileRandom& r, const TileRandom& dirtRandom) {
    paintDirt(img, dirtRandom);
    for (int x = 0; x < kTilePixels; ++x) {
        const int depth = 3 + (r.at(x, 0, 11) < 0.5f ? 0 : 1) + (r.at(x, 0, 12) < 0.25f ? 1 : 0);
        for (int y = 0; y < depth; ++y) {
            px(img, x, y) = shade(kGrass, 0.88f + 0.24f * r.at(x, y));
        }
        if (r.at(x, 1, 13) < 0.5f) {
            px(img, x, depth) = shade(kGrass, 0.7f);
        }
    }
}

void paintWoodSide(Image16& img, const TileRandom& r) {
    const Rgba bark = rgb(104, 82, 52);
    for (int x = 0; x < kTilePixels; ++x) {
        const float column = (r.at(x, 0, 2) < 0.3f) ? 0.72f : 0.95f + 0.12f * r.at(x, 1, 2);
        for (int y = 0; y < kTilePixels; ++y) {
            const float groove = (r.at(x, y / 4, 4) < 0.12f) ? 0.8f : 1.0f;
            px(img, x, y) = shade(bark, column * groove * (0.92f + 0.14f * r.at(x, y)));
        }
    }
}

void paintWoodTop(Image16& img, const TileRandom& r) {
    const Rgba light = rgb(184, 148, 94);
    const Rgba dark = rgb(150, 116, 72);
    const Rgba bark = rgb(104, 82, 52);
    for (int y = 0; y < kTilePixels; ++y) {
        for (int x = 0; x < kTilePixels; ++x) {
            const float dx = static_cast<float>(x) - 7.5f;
            const float dy = static_cast<float>(y) - 7.5f;
            const float ring = std::fmod(std::sqrt(dx * dx + dy * dy), 2.6f);
            Rgba c = ring < 1.3f ? light : dark;
            if (x == 0 || y == 0 || x == kTilePixels - 1 || y == kTilePixels - 1) {
                c = bark;
            }
            px(img, x, y) = shade(c, 0.94f + 0.12f * r.at(x, y));
        }
    }
}

void paintLeaves(Image16& img, const TileRandom& r) {
    const Rgba leaf = rgb(58, 128, 40);
    for (int y = 0; y < kTilePixels; ++y) {
        for (int x = 0; x < kTilePixels; ++x) {
            const float v = r.at(x, y);
            float f = 0.7f + 0.55f * r.smooth(x, y, 2, 1) + 0.2f * (v - 0.5f);
            if (v < 0.14f) {
                f = 0.42f; // deep gaps between leaves
            } else if (v > 0.92f) {
                f = 1.3f;
            }
            px(img, x, y) = shade(leaf, f);
        }
    }
}

void paintWater(Image16& img, const TileRandom& r) {
    const Rgba water = rgb(48, 98, 205);
    for (int y = 0; y < kTilePixels; ++y) {
        for (int x = 0; x < kTilePixels; ++x) {
            const float wave = std::sin((static_cast<float>(x) + static_cast<float>(y) * 0.5f) * 0.785398f);
            px(img, x, y) = shade(water, 0.9f + 0.1f * wave + 0.08f * (r.at(x, y) - 0.5f));
        }
    }
}

void paintGlass(Image16& img, const TileRandom& r) {
    for (auto& p : img) {
        p = rgb(0, 0, 0, 0);
    }
    const Rgba frame = rgb(205, 232, 240);
    for (int i = 0; i < kTilePixels; ++i) {
        const float f = 0.9f + 0.1f * r.at(i, 0);
        px(img, i, 0) = shade(frame, f);
        px(img, i, kTilePixels - 1) = shade(frame, f * 0.85f);
        px(img, 0, i) = shade(frame, f);
        px(img, kTilePixels - 1, i) = shade(frame, f * 0.85f);
    }
    // A couple of diagonal glints.
    for (int k = 0; k < 4; ++k) {
        px(img, 3 + k, 6 - k) = rgb(240, 250, 255, 200);
        px(img, 9 + k, 13 - k) = rgb(240, 250, 255, 170);
    }
}

void paintOre(Image16& img, const TileRandom& r, const TileRandom& stoneRandom, Rgba ore) {
    paintStone(img, stoneRandom);
    SplitMix64 rng = r.rng();
    const int clusters = 5;
    for (int c = 0; c < clusters; ++c) {
        const int cx = 2 + static_cast<int>(rng.nextBelow(12));
        const int cy = 2 + static_cast<int>(rng.nextBelow(12));
        const int size = 3 + static_cast<int>(rng.nextBelow(3));
        int x = cx;
        int y = cy;
        for (int k = 0; k < size; ++k) {
            px(img, x, y) = shade(ore, 0.85f + 0.3f * rng.nextFloat());
            x = std::clamp(x + static_cast<int>(rng.nextBelow(3)) - 1, 1, kTilePixels - 2);
            y = std::clamp(y + static_cast<int>(rng.nextBelow(3)) - 1, 1, kTilePixels - 2);
        }
    }
}

void paintBrick(Image16& img, const TileRandom& r) {
    const Rgba mortar = rgb(182, 176, 166);
    const Rgba brick = rgb(152, 64, 46);
    for (int y = 0; y < kTilePixels; ++y) {
        const int row = y / 4;
        for (int x = 0; x < kTilePixels; ++x) {
            const int shifted = x + (row % 2) * 4;
            if (y % 4 == 3 || shifted % 8 == 7) {
                px(img, x, y) = shade(mortar, 0.92f + 0.12f * r.at(x, y));
            } else {
                const float perBrick = 0.88f + 0.2f * r.at(shifted / 8, row, 9);
                px(img, x, y) = shade(brick, perBrick * (0.9f + 0.18f * r.at(x, y)));
            }
        }
    }
}

void paintBedrock(Image16& img, const TileRandom& r) {
    for (int y = 0; y < kTilePixels; ++y) {
        for (int x = 0; x < kTilePixels; ++x) {
            px(img, x, y) = shade(rgb(88, 88, 90), 0.35f + 0.95f * r.smooth(x, y, 2) * (0.7f + 0.6f * r.at(x, y)));
        }
    }
}

void paintPlanks(Image16& img, const TileRandom& r) {
    const Rgba wood = rgb(168, 134, 82);
    for (int y = 0; y < kTilePixels; ++y) {
        const int row = y / 4;
        const int seam = (row * 5 + 3) % kTilePixels;
        for (int x = 0; x < kTilePixels; ++x) {
            float f = 0.9f + 0.14f * r.at(x / 3, y, 6) + 0.05f * (r.at(x, y) - 0.5f);
            if (y % 4 == 3) {
                f = 0.62f;
            } else if (x == seam) {
                f = 0.7f;
            }
            px(img, x, y) = shade(wood, f);
        }
    }
}

void paintCobblestone(Image16& img, const TileRandom& r) {
    constexpr int kCells = 9;
    float cellX[kCells];
    float cellY[kCells];
    SplitMix64 rng = r.rng();
    for (int i = 0; i < kCells; ++i) {
        cellX[i] = rng.nextFloat() * kTilePixels;
        cellY[i] = rng.nextFloat() * kTilePixels;
    }
    for (int y = 0; y < kTilePixels; ++y) {
        for (int x = 0; x < kTilePixels; ++x) {
            float best = 1e9f;
            float second = 1e9f;
            int bestCell = 0;
            for (int i = 0; i < kCells; ++i) {
                // Wrap-around distance so the texture tiles seamlessly.
                float dx = std::fabs(static_cast<float>(x) + 0.5f - cellX[i]);
                float dy = std::fabs(static_cast<float>(y) + 0.5f - cellY[i]);
                dx = std::min(dx, kTilePixels - dx);
                dy = std::min(dy, kTilePixels - dy);
                const float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < best) {
                    second = best;
                    best = dist;
                    bestCell = i;
                } else if (dist < second) {
                    second = dist;
                }
            }
            if (second - best < 1.1f) {
                px(img, x, y) = shade(rgb(72, 72, 74), 0.9f + 0.2f * r.at(x, y));
            } else {
                const float cellShade = 0.8f + 0.35f * r.at(bestCell, 0, 8);
                px(img, x, y) = shade(rgb(122, 122, 124), cellShade * (0.92f + 0.14f * r.at(x, y)));
            }
        }
    }
}

void paintCactusSide(Image16& img, const TileRandom& r) {
    const Rgba green = rgb(78, 148, 58);
    for (int y = 0; y < kTilePixels; ++y) {
        for (int x = 0; x < kTilePixels; ++x) {
            float f = 0.92f + 0.14f * r.at(x, y);
            if (x % 4 == 0) {
                f *= 0.74f;
            }
            if (x == 0 || x == kTilePixels - 1) {
                f *= 0.8f;
            }
            Rgba c = shade(green, f);
            if (x % 4 == 2 && r.at(x, y, 4) < 0.18f) {
                c = rgb(222, 230, 180); // spines
            }
            px(img, x, y) = c;
        }
    }
}

void paintCactusTop(Image16& img, const TileRandom& r) {
    const Rgba green = rgb(96, 166, 70);
    for (int y = 0; y < kTilePixels; ++y) {
        for (int x = 0; x < kTilePixels; ++x) {
            const int edge = std::min(std::min(x, y), std::min(kTilePixels - 1 - x, kTilePixels - 1 - y));
            float f = edge == 0 ? 0.7f : (edge < 3 ? 0.95f : 1.08f);
            if ((x == 7 || x == 8) != (y == 7 || y == 8)) {
                f *= 0.85f;
            }
            px(img, x, y) = shade(green, f * (0.94f + 0.1f * r.at(x, y)));
        }
    }
}

void paintGravel(Image16& img, const TileRandom& r) {
    for (int y = 0; y < kTilePixels; ++y) {
        for (int x = 0; x < kTilePixels; ++x) {
            const float pebble = r.smooth(x, y, 2, 3);
            const Rgba base = r.at(x / 2, y / 2, 5) < 0.3f ? rgb(140, 120, 104) : rgb(132, 128, 126);
            px(img, x, y) = shade(base, 0.6f + 0.6f * pebble + 0.1f * (r.at(x, y) - 0.5f));
        }
    }
}

void paintSand(Image16& img, const TileRandom& r) {
    const Rgba sand = rgb(220, 208, 160);
    noisyFill(img, r, sand, 0.06f, 0.03f);
    for (int y = 0; y < kTilePixels; ++y) {
        for (int x = 0; x < kTilePixels; ++x) {
            if (r.at(x, y, 2) < 0.07f) {
                px(img, x, y) = shade(sand, 0.85f);
            }
        }
    }
}

void paintSnow(Image16& img, const TileRandom& r) {
    noisyFill(img, r, rgb(238, 245, 252), 0.035f, 0.025f);
}

Image16 paintTile(Tile tile) {
    Image16 img{};
    const TileRandom r(tile);
    switch (tile) {
    case Tile::GrassTop: paintGrassTop(img, r); break;
    case Tile::GrassSide: paintGrassSide(img, r, TileRandom(Tile::Dirt)); break;
    case Tile::Dirt: paintDirt(img, r); break;
    case Tile::Stone: paintStone(img, r); break;
    case Tile::Sand: paintSand(img, r); break;
    case Tile::Snow: paintSnow(img, r); break;
    case Tile::WoodSide: paintWoodSide(img, r); break;
    case Tile::WoodTop: paintWoodTop(img, r); break;
    case Tile::Leaves: paintLeaves(img, r); break;
    case Tile::Water: paintWater(img, r); break;
    case Tile::Glass: paintGlass(img, r); break;
    case Tile::CoalOre: paintOre(img, r, TileRandom(Tile::Stone), rgb(38, 38, 40)); break;
    case Tile::IronOre: paintOre(img, r, TileRandom(Tile::Stone), rgb(214, 172, 140)); break;
    case Tile::Brick: paintBrick(img, r); break;
    case Tile::Bedrock: paintBedrock(img, r); break;
    case Tile::Planks: paintPlanks(img, r); break;
    case Tile::Cobblestone: paintCobblestone(img, r); break;
    case Tile::GoldOre: paintOre(img, r, TileRandom(Tile::Stone), rgb(250, 214, 60)); break;
    case Tile::DiamondOre: paintOre(img, r, TileRandom(Tile::Stone), rgb(92, 232, 222)); break;
    case Tile::CactusSide: paintCactusSide(img, r); break;
    case Tile::CactusTop: paintCactusTop(img, r); break;
    case Tile::Gravel: paintGravel(img, r); break;
    case Tile::Count: break;
    }
    return img;
}

} // namespace

std::size_t TextureAtlas::levelOffset(int level) {
    std::size_t offset = 0;
    for (int l = 0; l < level; ++l) {
        offset += static_cast<std::size_t>(width(l)) * static_cast<std::size_t>(height(l)) * 4u;
    }
    return offset;
}

Rgba TextureAtlas::pixel(int x, int y, int level) const {
    const std::size_t i = levelOffset(level) + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width(level)) +
                                                static_cast<std::size_t>(x)) * 4u;
    return {m_data[i], m_data[i + 1], m_data[i + 2], m_data[i + 3]};
}

Rgba TextureAtlas::tilePixel(Tile tile, int px_, int py_) const {
    return pixel(tileX(tile) + px_, tileY(tile) + py_);
}

void TextureAtlas::setPixel(int x, int y, Rgba color) {
    const std::size_t i = (static_cast<std::size_t>(y) * kAtlasWidth + static_cast<std::size_t>(x)) * 4u;
    m_data[i] = color.r;
    m_data[i + 1] = color.g;
    m_data[i + 2] = color.b;
    m_data[i + 3] = color.a;
}

void TextureAtlas::buildMipChain() {
    for (int level = 1; level < kAtlasMipLevels; ++level) {
        const std::size_t src = levelOffset(level - 1);
        const std::size_t dst = levelOffset(level);
        const int srcW = width(level - 1);
        const int srcH = height(level - 1);
        const int w = width(level);
        const int h = height(level);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float sum[3] = {0, 0, 0};
                float alpha = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    const int sx = std::min(x * 2 + (k & 1), srcW - 1);
                    const int sy = std::min(y * 2 + (k >> 1), srcH - 1);
                    const std::size_t i = src + (static_cast<std::size_t>(sy) * static_cast<std::size_t>(srcW) +
                                                 static_cast<std::size_t>(sx)) * 4u;
                    const float a = m_data[i + 3];
                    for (int c = 0; c < 3; ++c) {
                        sum[c] += static_cast<float>(m_data[i + static_cast<std::size_t>(c)]) * a;
                    }
                    alpha += a;
                }
                const std::size_t o = dst + (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                                             static_cast<std::size_t>(x)) * 4u;
                for (int c = 0; c < 3; ++c) {
                    m_data[o + static_cast<std::size_t>(c)] =
                        alpha > 0.0f ? static_cast<std::uint8_t>(sum[c] / alpha + 0.5f) : 0;
                }
                m_data[o + 3] = static_cast<std::uint8_t>(alpha / 4.0f + 0.5f);
            }
        }
    }
}

TextureAtlas TextureAtlas::generate() {
    TextureAtlas atlas;
    atlas.m_data.assign(atlas.levelOffset(kAtlasMipLevels), 0);
    for (int t = 0; t < kTileCount; ++t) {
        const auto tile = static_cast<Tile>(t);
        const Image16 img = paintTile(tile);
        for (int y = 0; y < kTilePixels; ++y) {
            for (int x = 0; x < kTilePixels; ++x) {
                atlas.setPixel(tileX(tile) + x, tileY(tile) + y, img[static_cast<std::size_t>(y * kTilePixels + x)]);
            }
        }
    }
    atlas.buildMipChain();
    return atlas;
}

} // namespace voxelforge
