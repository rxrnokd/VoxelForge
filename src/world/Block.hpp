#pragma once

#include <array>
#include <cstdint>

namespace voxelforge {

// Block ids are stored in chunks as single bytes. The numeric values are part of the
// save file format: append new blocks at the end, never reorder.
enum class BlockType : std::uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Sand,
    Snow,
    Wood,
    Leaves,
    Water,
    Glass,
    CoalOre,
    IronOre,
    Brick,
    Bedrock,
    Planks,
    Cobblestone,
    GoldOre,
    DiamondOre,
    Cactus,
    Gravel,
    Count
};

inline constexpr int kBlockTypeCount = static_cast<int>(BlockType::Count);

// Texture atlas tiles. Each tile is a procedurally generated 16x16 image.
enum class Tile : std::uint8_t {
    GrassTop = 0,
    GrassSide,
    Dirt,
    Stone,
    Sand,
    Snow,
    WoodSide,
    WoodTop,
    Leaves,
    Water,
    Glass,
    CoalOre,
    IronOre,
    Brick,
    Bedrock,
    Planks,
    Cobblestone,
    GoldOre,
    DiamondOre,
    CactusSide,
    CactusTop,
    Gravel,
    Count
};

inline constexpr int kTileCount = static_cast<int>(Tile::Count);

// Cube face directions. The order matters: the mesher and shaders index normals by it.
enum class Face : std::uint8_t { NegX = 0, PosX, NegY, PosY, NegZ, PosZ };

struct BlockInfo {
    const char* name;
    Tile top;
    Tile side;
    Tile bottom;
    bool solid;       // collides with entities
    bool opaque;      // fully hides adjacent faces and darkens ambient occlusion
    bool translucent; // rendered in the blended (water) pass
    bool selectable;  // can be targeted by the block raycast
};

namespace detail {
// clang-format off
inline constexpr std::array<BlockInfo, kBlockTypeCount> kBlockTable{{
    //  name           top               side              bottom          solid  opaque translucent selectable
    {"Air",          Tile::Dirt,       Tile::Dirt,       Tile::Dirt,       false, false, false, false},
    {"Grass",        Tile::GrassTop,   Tile::GrassSide,  Tile::Dirt,       true,  true,  false, true},
    {"Dirt",         Tile::Dirt,       Tile::Dirt,       Tile::Dirt,       true,  true,  false, true},
    {"Stone",        Tile::Stone,      Tile::Stone,      Tile::Stone,      true,  true,  false, true},
    {"Sand",         Tile::Sand,       Tile::Sand,       Tile::Sand,       true,  true,  false, true},
    {"Snow",         Tile::Snow,       Tile::Snow,       Tile::Snow,       true,  true,  false, true},
    {"Wood",         Tile::WoodTop,    Tile::WoodSide,   Tile::WoodTop,    true,  true,  false, true},
    {"Leaves",       Tile::Leaves,     Tile::Leaves,     Tile::Leaves,     true,  true,  false, true},
    {"Water",        Tile::Water,      Tile::Water,      Tile::Water,      false, false, true,  false},
    {"Glass",        Tile::Glass,      Tile::Glass,      Tile::Glass,      true,  false, false, true},
    {"Coal Ore",     Tile::CoalOre,    Tile::CoalOre,    Tile::CoalOre,    true,  true,  false, true},
    {"Iron Ore",     Tile::IronOre,    Tile::IronOre,    Tile::IronOre,    true,  true,  false, true},
    {"Brick",        Tile::Brick,      Tile::Brick,      Tile::Brick,      true,  true,  false, true},
    {"Bedrock",      Tile::Bedrock,    Tile::Bedrock,    Tile::Bedrock,    true,  true,  false, true},
    {"Planks",       Tile::Planks,     Tile::Planks,     Tile::Planks,     true,  true,  false, true},
    {"Cobblestone",  Tile::Cobblestone,Tile::Cobblestone,Tile::Cobblestone,true,  true,  false, true},
    {"Gold Ore",     Tile::GoldOre,    Tile::GoldOre,    Tile::GoldOre,    true,  true,  false, true},
    {"Diamond Ore",  Tile::DiamondOre, Tile::DiamondOre, Tile::DiamondOre, true,  true,  false, true},
    {"Cactus",       Tile::CactusTop,  Tile::CactusSide, Tile::CactusTop,  true,  true,  false, true},
    {"Gravel",       Tile::Gravel,     Tile::Gravel,     Tile::Gravel,     true,  true,  false, true},
}};
// clang-format on
} // namespace detail

inline constexpr const BlockInfo& blockInfo(BlockType type) {
    return detail::kBlockTable[static_cast<std::size_t>(type)];
}
inline constexpr bool isSolid(BlockType t) { return blockInfo(t).solid; }
inline constexpr bool isOpaque(BlockType t) { return blockInfo(t).opaque; }
inline constexpr bool isTranslucent(BlockType t) { return blockInfo(t).translucent; }
inline constexpr bool isSelectable(BlockType t) { return blockInfo(t).selectable; }
inline constexpr bool isValidBlock(std::uint8_t raw) { return raw < kBlockTypeCount; }

// Atlas tile used by the given face of a block.
inline constexpr Tile blockTile(BlockType type, Face face) {
    const BlockInfo& info = blockInfo(type);
    switch (face) {
    case Face::PosY: return info.top;
    case Face::NegY: return info.bottom;
    default: return info.side;
    }
}

const char* blockName(BlockType type);

} // namespace voxelforge
