#include "doctest.h"

#include "render/Mesher.hpp"
#include "world/TerrainGenerator.hpp"

#include <map>
#include <memory>
#include <set>

using namespace voxelforge;

namespace {

ChunkNeighborhood alone(const Chunk& c) {
    ChunkNeighborhood n;
    n.chunks[4] = &c;
    return n;
}

int quads(const ChunkMesh& m) { return static_cast<int>(m.vertexCount() / 6); }

Vec3 position(const ChunkVertex& v) {
    return {static_cast<float>(v.x) / 16.0f, static_cast<float>(v.y) / 16.0f, static_cast<float>(v.z) / 16.0f};
}

const Vec3 kNormals[6] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};

} // namespace

TEST_CASE("mesher: empty chunk produces no geometry") {
    auto chunk = std::make_unique<Chunk>();
    ChunkMesher mesher;
    ChunkMesh mesh;
    mesher.build(alone(*chunk), mesh);
    CHECK(mesh.empty());
}

TEST_CASE("mesher: single block has six faces in both modes") {
    auto chunk = std::make_unique<Chunk>();
    chunk->set(5, 10, 5, BlockType::Stone);
    ChunkMesher mesher;
    ChunkMesh mesh;
    mesher.build(alone(*chunk), mesh, MeshingMode::Culled);
    CHECK(quads(mesh) == 6);
    mesher.build(alone(*chunk), mesh, MeshingMode::Greedy);
    CHECK(quads(mesh) == 6);
    CHECK(mesh.minY == 10);
    CHECK(mesh.maxY == 11);
}

TEST_CASE("mesher: hidden faces are removed and greedy merging reduces quads") {
    auto chunk = std::make_unique<Chunk>();
    for (int y = 1; y <= 3; ++y)
        for (int z = 1; z <= 3; ++z)
            for (int x = 1; x <= 3; ++x)
                chunk->set(x, y, z, BlockType::Stone);
    ChunkMesher mesher;
    ChunkMesh mesh;
    mesher.build(alone(*chunk), mesh, MeshingMode::Culled);
    CHECK(quads(mesh) == 6 * 9); // only the outer shell: 9 faces per side
    mesher.build(alone(*chunk), mesh, MeshingMode::Greedy);
    CHECK(quads(mesh) == 6); // each side collapses into a single quad
}

TEST_CASE("mesher: a full flat layer becomes very few quads") {
    auto chunk = std::make_unique<Chunk>();
    for (int z = 0; z < kChunkSize; ++z)
        for (int x = 0; x < kChunkSize; ++x)
            chunk->set(x, 0, z, BlockType::Grass);
    ChunkMesher mesher;
    ChunkMesh mesh;
    mesher.build(alone(*chunk), mesh, MeshingMode::Greedy);
    // Top + 4 sides (bottom is culled against the world floor).
    CHECK(quads(mesh) == 5);
    mesher.build(alone(*chunk), mesh, MeshingMode::Culled);
    CHECK(quads(mesh) == 256 + 4 * 16);
}

TEST_CASE("mesher: different block types are not merged") {
    auto chunk = std::make_unique<Chunk>();
    chunk->set(0, 5, 0, BlockType::Stone);
    chunk->set(1, 5, 0, BlockType::Dirt);
    ChunkMesher mesher;
    ChunkMesh mesh;
    mesher.build(alone(*chunk), mesh, MeshingMode::Greedy);
    CHECK(quads(mesh) == 10);
}

TEST_CASE("mesher: neighbouring chunks hide border faces") {
    auto center = std::make_unique<Chunk>();
    auto east = std::make_unique<Chunk>();
    center->set(15, 20, 8, BlockType::Stone);
    east->set(0, 20, 8, BlockType::Stone);
    ChunkMesher mesher;
    ChunkMesh mesh;
    ChunkNeighborhood n = alone(*center);
    mesher.build(n, mesh);
    CHECK(quads(mesh) == 6);
    n.at(1, 0) = east.get();
    mesher.build(n, mesh);
    CHECK(quads(mesh) == 5);
}

TEST_CASE("mesher: transparent blocks") {
    auto chunk = std::make_unique<Chunk>();
    chunk->set(4, 4, 4, BlockType::Glass);
    chunk->set(5, 4, 4, BlockType::Glass);
    chunk->set(4, 4, 6, BlockType::Stone);
    chunk->set(4, 4, 7, BlockType::Glass);
    ChunkMesher mesher;
    ChunkMesh mesh;
    mesher.build(alone(*chunk), mesh, MeshingMode::Culled);
    // Two touching glass blocks hide their shared face (10 faces). The stone keeps the
    // face it shares with glass because glass is see-through (6 faces), while the glass
    // face against the opaque stone is hidden (5 faces).
    CHECK(quads(mesh) == 10 + 6 + 5);
}

TEST_CASE("mesher: water goes to its own buffer with a lowered surface") {
    auto chunk = std::make_unique<Chunk>();
    for (int x = 0; x < 4; ++x) {
        chunk->set(x, 10, 0, BlockType::Stone);
        chunk->set(x, 11, 0, BlockType::Water);
        chunk->set(x, 12, 0, BlockType::Water);
    }
    ChunkMesher mesher;
    ChunkMesh mesh;
    mesher.build(alone(*chunk), mesh);
    CHECK_FALSE(mesh.water.empty());
    int topY = 0;
    for (const ChunkVertex& v : mesh.water) {
        topY = std::max<int>(topY, v.y);
        CHECK(v.ao() == 3);
    }
    CHECK(topY == 13 * 16 - 2); // 2/16 below the top of the upper water block
    // The stone top is hidden by water? No: water is not opaque, so it stays visible.
    bool stoneTop = false;
    for (const ChunkVertex& v : mesh.opaque) {
        stoneTop |= v.face() == Face::PosY && v.y == 11 * 16;
    }
    CHECK(stoneTop);
}

TEST_CASE("mesher: triangles wind counter-clockwise around the face normal") {
    const TerrainGenerator gen(99);
    auto chunk = std::make_unique<Chunk>();
    gen.generate({0, 0}, *chunk);
    chunk->set(8, 120, 8, BlockType::Brick); // floating block exercises all six faces
    ChunkMesher mesher;
    ChunkMesh mesh;
    mesher.build(alone(*chunk), mesh);
    REQUIRE(mesh.opaque.size() % 3 == 0);
    std::set<int> faces;
    for (std::size_t i = 0; i < mesh.opaque.size(); i += 3) {
        const Vec3 a = position(mesh.opaque[i]);
        const Vec3 b = position(mesh.opaque[i + 1]);
        const Vec3 c = position(mesh.opaque[i + 2]);
        const Vec3 n = cross(b - a, c - a);
        const int face = static_cast<int>(mesh.opaque[i].face());
        faces.insert(face);
        CHECK(dot(n, kNormals[face]) > 0.0f);
        // All vertices stay inside the chunk column.
        CHECK(a.x >= 0.0f);
        CHECK(a.x <= 16.0f);
        CHECK(a.z >= 0.0f);
        CHECK(a.z <= 16.0f);
    }
    CHECK(faces.size() == 6);
}

TEST_CASE("mesher: ambient occlusion darkens corners next to walls") {
    auto chunk = std::make_unique<Chunk>();
    chunk->set(5, 10, 5, BlockType::Stone); // floor block
    chunk->set(6, 11, 5, BlockType::Stone); // wall block touching the floor's +X top edge
    ChunkMesher mesher;
    ChunkMesh mesh;
    mesher.build(alone(*chunk), mesh, MeshingMode::Culled);
    int dark = 0;
    int bright = 0;
    for (const ChunkVertex& v : mesh.opaque) {
        if (v.face() == Face::PosY && v.y == 11 * 16 && v.x <= 6 * 16 && v.x >= 5 * 16 && v.z <= 6 * 16 &&
            v.z >= 5 * 16 && !(v.x == 6 * 16 && v.y != 11 * 16)) {
            if (v.x == 6 * 16) {
                dark += v.ao() < 3 ? 1 : 0;
            } else {
                bright += v.ao() == 3 ? 1 : 0;
            }
        }
    }
    CHECK(dark > 0);
    CHECK(bright > 0);
}

TEST_CASE("mesher: greedy meshing of real terrain is much smaller than culled") {
    const TerrainGenerator gen(2024);
    std::map<std::pair<int, int>, std::unique_ptr<Chunk>> chunks;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            auto c = std::make_unique<Chunk>();
            gen.generate({dx, dz}, *c);
            chunks[{dx, dz}] = std::move(c);
        }
    ChunkNeighborhood n;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            n.at(dx, dz) = chunks[{dx, dz}].get();
    ChunkMesher mesher;
    ChunkMesh culled;
    ChunkMesh greedy;
    mesher.build(n, culled, MeshingMode::Culled);
    mesher.build(n, greedy, MeshingMode::Greedy);
    CHECK(greedy.vertexCount() > 0);
    CHECK(greedy.vertexCount() * 2 < culled.vertexCount());
}
