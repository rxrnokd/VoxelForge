#include "render/Mesher.hpp"

#include <algorithm>
#include <cstring>

namespace voxelforge {
namespace {

// Face keys pack everything that must be equal for two faces to merge:
//   bits 0-7   block type
//   bits 8-15  ambient occlusion of the four corners (2 bits each)
//   bit  16    "lowered" water surface (block above is not water)
constexpr std::uint32_t kAoShift = 8;
constexpr std::uint32_t kLoweredBit = 1u << 16;
constexpr std::uint32_t kFullBrightAo = 0xFFu << kAoShift;
constexpr int kWaterDrop = 2; // water surface sits 2/16 below the block top

// Whether the face of `self` that touches `neighbor` can be seen.
inline bool faceVisible(BlockType self, BlockType neighbor) {
    if (neighbor == BlockType::Air) {
        return true;
    }
    if (isOpaque(neighbor)) {
        return false;
    }
    return neighbor != self; // glass/glass and water/water interfaces are hidden
}

// Classic voxel AO (0fps.net): two solid sides fully occlude the corner.
inline int vertexAo(int side1, int side2, int corner) {
    return (side1 != 0 && side2 != 0) ? 0 : 3 - (side1 + side2 + corner);
}

constexpr int kAxisStride[3] = {1, ChunkMesher::kPadX * ChunkMesher::kPadZ, ChunkMesher::kPadX};

} // namespace

ChunkMesher::ChunkMesher()
    : m_padded(static_cast<std::size_t>(kPadX * kPadY * kPadZ), BlockType::Air),
      m_mask(static_cast<std::size_t>(kChunkHeight * kChunkSize), 0u) {}

void ChunkMesher::copyNeighborhood(const ChunkNeighborhood& neighborhood, int topY) {
    // Layer below the world: treat as solid so the underside of bedrock is never meshed.
    std::fill_n(m_padded.begin() + padIndex(-1, -1, -1), kPadX * kPadZ, BlockType::Bedrock);

    const int copyTop = std::min(topY, kChunkHeight - 1);
    for (int dz = -1; dz <= 1; ++dz) {
        const int z0 = dz < 0 ? -1 : (dz == 0 ? 0 : kChunkSize);
        const int z1 = dz < 0 ? -1 : (dz == 0 ? kChunkSize - 1 : kChunkSize);
        for (int dx = -1; dx <= 1; ++dx) {
            const int x0 = dx < 0 ? -1 : (dx == 0 ? 0 : kChunkSize);
            const int x1 = dx < 0 ? -1 : (dx == 0 ? kChunkSize - 1 : kChunkSize);
            const auto width = static_cast<std::size_t>(x1 - x0 + 1);
            const Chunk* chunk = neighborhood.chunks[static_cast<std::size_t>((dz + 1) * 3 + (dx + 1))];
            for (int y = 0; y <= copyTop; ++y) {
                for (int z = z0; z <= z1; ++z) {
                    BlockType* dst = &m_padded[static_cast<std::size_t>(padIndex(x0, y, z))];
                    if (chunk != nullptr) {
                        const int srcIndex = Chunk::index(x0 - dx * kChunkSize, y, z - dz * kChunkSize);
                        std::memcpy(dst, &chunk->storage()[static_cast<std::size_t>(srcIndex)], width);
                    } else {
                        std::fill_n(dst, width, BlockType::Air);
                    }
                }
            }
        }
    }
    if (topY >= kChunkHeight) { // padding layer above the world
        std::fill_n(m_padded.begin() + padIndex(-1, kChunkHeight, -1), kPadX * kPadZ, BlockType::Air);
    }
}

void ChunkMesher::build(const ChunkNeighborhood& neighborhood, ChunkMesh& out, MeshingMode mode) {
    out.clear();
    const Chunk* center = neighborhood.center();
    if (center == nullptr) {
        return;
    }
    const int maxY = center->highestNonAir();
    if (maxY < 0) {
        return;
    }
    copyNeighborhood(neighborhood, maxY + 1);

    out.minY = kChunkHeight;
    out.maxY = 0;
    for (int face = 0; face < 6; ++face) {
        meshFace(face, maxY, out, mode);
    }
    if (out.quadCount == 0) {
        out.minY = 0;
        out.maxY = 0;
    }
}

void ChunkMesher::meshFace(int face, int maxY, ChunkMesh& out, MeshingMode mode) {
    const int d = face >> 1;
    const bool positive = (face & 1) != 0;
    const int u = (d + 1) % 3;
    const int v = (d + 2) % 3;
    const int dims[3] = {kChunkSize, maxY + 1, kChunkSize};
    const int du = dims[u];
    const int dv = dims[v];
    const int su = kAxisStride[u];
    const int sv = kAxisStride[v];
    const int neighborOffset = positive ? kAxisStride[d] : -kAxisStride[d];
    const int up = kAxisStride[1];
    const BlockType* blocks = m_padded.data();
    std::uint32_t* mask = m_mask.data();
    const bool greedy = mode == MeshingMode::Greedy;

    for (int slice = 0; slice < dims[d]; ++slice) {
        // 1. Build the mask of visible faces for this slice.
        bool anyFace = false;
        const int sliceBase = padIndex(0, 0, 0) + slice * kAxisStride[d];
        for (int j = 0; j < dv; ++j) {
            int index = sliceBase + j * sv;
            std::uint32_t* maskRow = mask + j * du;
            for (int i = 0; i < du; ++i, index += su) {
                const BlockType self = blocks[index];
                std::uint32_t key = 0;
                if (self != BlockType::Air) {
                    const int q = index + neighborOffset;
                    if (faceVisible(self, blocks[q])) {
                        if (isTranslucent(self)) {
                            key = static_cast<std::uint32_t>(self) | kFullBrightAo;
                            if (blocks[index + up] != self) {
                                key |= kLoweredBit;
                            }
                        } else {
                            auto occ = [blocks](int at) { return isOpaque(blocks[at]) ? 1 : 0; };
                            const int sNegU = occ(q - su);
                            const int sPosU = occ(q + su);
                            const int sNegV = occ(q - sv);
                            const int sPosV = occ(q + sv);
                            const int ao0 = vertexAo(sNegU, sNegV, occ(q - su - sv));
                            const int ao1 = vertexAo(sPosU, sNegV, occ(q + su - sv));
                            const int ao2 = vertexAo(sPosU, sPosV, occ(q + su + sv));
                            const int ao3 = vertexAo(sNegU, sPosV, occ(q - su + sv));
                            key = static_cast<std::uint32_t>(self) |
                                  static_cast<std::uint32_t>(ao0 | (ao1 << 2) | (ao2 << 4) | (ao3 << 6)) << kAoShift;
                        }
                    }
                }
                maskRow[i] = key;
                anyFace |= key != 0;
            }
        }
        if (!anyFace) {
            continue;
        }

        // 2. Cover the mask with rectangles of identical keys.
        for (int j = 0; j < dv; ++j) {
            for (int i = 0; i < du;) {
                const std::uint32_t key = mask[i + j * du];
                if (key == 0) {
                    ++i;
                    continue;
                }
                int w = 1;
                int h = 1;
                if (greedy) {
                    while (i + w < du && mask[i + w + j * du] == key) {
                        ++w;
                    }
                    for (; j + h < dv; ++h) {
                        const std::uint32_t* row = mask + i + (j + h) * du;
                        bool same = true;
                        for (int k = 0; k < w; ++k) {
                            if (row[k] != key) {
                                same = false;
                                break;
                            }
                        }
                        if (!same) {
                            break;
                        }
                    }
                }
                emitQuad(face, slice, i, j, w, h, key, out);
                for (int r = 0; r < h; ++r) {
                    std::fill_n(mask + i + (j + r) * du, w, 0u);
                }
                i += w;
            }
        }
    }
}

void ChunkMesher::emitQuad(int face, int slice, int i, int j, int w, int h, std::uint32_t key,
                           ChunkMesh& out) const {
    const int d = face >> 1;
    const bool positive = (face & 1) != 0;
    const int u = (d + 1) % 3;
    const int v = (d + 2) % 3;

    // Corners in fixed point, counter-clockwise around +d: (0,0) (w,0) (w,h) (0,h).
    int corners[4][3];
    for (int k = 0; k < 4; ++k) {
        int c[3];
        c[d] = slice + (positive ? 1 : 0);
        c[u] = i + ((k == 1 || k == 2) ? w : 0);
        c[v] = j + ((k == 2 || k == 3) ? h : 0);
        for (int a = 0; a < 3; ++a) {
            corners[k][a] = c[a] * ChunkVertex::kSubUnits;
        }
    }

    if ((key & kLoweredBit) != 0) {
        if (d == 1 && positive) {
            for (auto& c : corners) {
                c[1] -= kWaterDrop;
            }
        } else if (d != 1) {
            int top = 0;
            for (const auto& c : corners) {
                top = std::max(top, c[1]);
            }
            for (auto& c : corners) {
                if (c[1] == top) {
                    c[1] -= kWaterDrop;
                }
            }
        }
    }

    const auto type = static_cast<BlockType>(key & 0xFFu);
    const Tile tile = blockTile(type, static_cast<Face>(face));
    int ao[4];
    for (int k = 0; k < 4; ++k) {
        ao[k] = static_cast<int>((key >> (kAoShift + 2u * static_cast<unsigned>(k))) & 3u);
    }

    // Negative faces reverse the winding so every quad is counter-clockwise from outside.
    static constexpr int kPositiveOrder[4] = {0, 1, 2, 3};
    static constexpr int kNegativeOrder[4] = {0, 3, 2, 1};
    const int* order = positive ? kPositiveOrder : kNegativeOrder;

    ChunkVertex quad[4];
    int quadAo[4];
    for (int k = 0; k < 4; ++k) {
        const int* c = corners[order[k]];
        quadAo[k] = ao[order[k]];
        quad[k].x = static_cast<std::uint16_t>(c[0]);
        quad[k].y = static_cast<std::uint16_t>(c[1]);
        quad[k].z = static_cast<std::uint16_t>(c[2]);
        quad[k].data = ChunkVertex::pack(static_cast<Face>(face), quadAo[k], tile);
    }

    // Split along the diagonal whose corners are brighter; otherwise AO gradients show
    // an obvious seam (anisotropy fix from 0fps.net).
    static constexpr int kDefaultSplit[6] = {0, 1, 2, 0, 2, 3};
    static constexpr int kFlippedSplit[6] = {1, 2, 3, 1, 3, 0};
    const bool flip = quadAo[0] + quadAo[2] < quadAo[1] + quadAo[3];
    const int* split = flip ? kFlippedSplit : kDefaultSplit;

    std::vector<ChunkVertex>& target = isTranslucent(type) ? out.water : out.opaque;
    for (int k = 0; k < 6; ++k) {
        target.push_back(quad[split[k]]);
    }

    int lowY = corners[0][1];
    int highY = corners[0][1];
    for (const auto& c : corners) {
        lowY = std::min(lowY, c[1]);
        highY = std::max(highY, c[1]);
    }
    out.minY = std::min(out.minY, lowY / ChunkVertex::kSubUnits);
    out.maxY = std::max(out.maxY, (highY + ChunkVertex::kSubUnits - 1) / ChunkVertex::kSubUnits);
    ++out.quadCount;
}

} // namespace voxelforge
