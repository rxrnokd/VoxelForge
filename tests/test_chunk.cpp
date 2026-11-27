#include "doctest.h"

#include "world/Chunk.hpp"

#include <set>
#include <string>

using namespace voxelforge;

TEST_CASE("chunk: index layout is unique and Y-major") {
    std::set<int> seen;
    for (int y = 0; y < kChunkHeight; y += 17) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                const int i = Chunk::index(x, y, z);
                CHECK(i >= 0);
                CHECK(i < kChunkVolume);
                CHECK(seen.insert(i).second);
            }
        }
    }
    CHECK(Chunk::index(1, 0, 0) - Chunk::index(0, 0, 0) == 1);
    CHECK(Chunk::index(0, 1, 0) - Chunk::index(0, 0, 0) == kChunkArea);
}

TEST_CASE("chunk: get/set/fill and bounds") {
    Chunk chunk;
    CHECK(chunk.get(3, 100, 7) == BlockType::Air);
    CHECK(chunk.highestNonAir() == -1);
    chunk.set(3, 100, 7, BlockType::Brick);
    CHECK(chunk.get(3, 100, 7) == BlockType::Brick);
    CHECK(chunk.highestNonAir() == 100);
    CHECK(chunk.getSafe(3, 300, 7) == BlockType::Air);
    CHECK(chunk.getSafe(3, -1, 7) == BlockType::Air);
    CHECK_FALSE(Chunk::inBounds(16, 0, 0));
    CHECK_FALSE(Chunk::inBounds(0, 256, 0));
    chunk.fill(BlockType::Stone);
    CHECK(chunk.get(15, 255, 15) == BlockType::Stone);
    CHECK(sizeof(Chunk) == static_cast<std::size_t>(kChunkVolume));
}

TEST_CASE("chunk: world to chunk coordinates") {
    CHECK(ChunkPos::fromWorld(0, 0) == ChunkPos{0, 0});
    CHECK(ChunkPos::fromWorld(15, 16) == ChunkPos{0, 1});
    CHECK(ChunkPos::fromWorld(-1, -16) == ChunkPos{-1, -1});
    CHECK(ChunkPos::fromWorld(-17, 33) == ChunkPos{-2, 2});
    CHECK(ChunkPos{-2, 3}.originX() == -32);
    ChunkPosHash hash;
    CHECK(hash(ChunkPos{1, 2}) != hash(ChunkPos{2, 1}));
}

TEST_CASE("blocks: property table sanity") {
    CHECK(kBlockTypeCount >= 13);
    CHECK(isOpaque(BlockType::Stone));
    CHECK_FALSE(isOpaque(BlockType::Glass));
    CHECK(isSolid(BlockType::Glass));
    CHECK_FALSE(isSolid(BlockType::Water));
    CHECK(isTranslucent(BlockType::Water));
    CHECK_FALSE(isSelectable(BlockType::Water));
    CHECK(blockTile(BlockType::Grass, Face::PosY) == Tile::GrassTop);
    CHECK(blockTile(BlockType::Grass, Face::NegX) == Tile::GrassSide);
    CHECK(blockTile(BlockType::Grass, Face::NegY) == Tile::Dirt);
    CHECK(std::string(blockName(BlockType::IronOre)) == "Iron Ore");
}
