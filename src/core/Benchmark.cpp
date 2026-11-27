#include "core/Benchmark.hpp"

#include "render/Mesher.hpp"
#include "world/TerrainGenerator.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>

namespace voxelforge {
namespace {

using Clock = std::chrono::steady_clock;

double millisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

struct Summary {
    double total = 0.0;
    double min = 1e30;
    double max = 0.0;
    int samples = 0;

    void add(double v) {
        total += v;
        min = std::min(min, v);
        max = std::max(max, v);
        ++samples;
    }
    double mean() const { return samples > 0 ? total / samples : 0.0; }
};

struct MeshTotals {
    Summary timeMs;
    std::int64_t vertices = 0;
    std::int64_t quads = 0;
};

} // namespace

int runBenchmark(const BenchmarkOptions& options) {
    const int side = options.quick ? 8 : 16; // generated region is side x side chunks
    const TerrainGenerator generator(options.seed);

    std::printf("VoxelForge benchmark (seed %llu, %dx%d chunks, single thread)\n",
                static_cast<unsigned long long>(options.seed), side, side);
    std::printf("------------------------------------------------------------------\n");

    // 1. Terrain generation.
    std::vector<std::unique_ptr<Chunk>> chunks(static_cast<std::size_t>(side * side));
    Summary generation;
    for (int z = 0; z < side; ++z) {
        for (int x = 0; x < side; ++x) {
            auto chunk = std::make_unique<Chunk>();
            const auto start = Clock::now();
            generator.generate({x, z}, *chunk);
            generation.add(millisecondsSince(start));
            chunks[static_cast<std::size_t>(z * side + x)] = std::move(chunk);
        }
    }
    std::printf("Chunk generation   : %7.3f ms/chunk  (min %.3f, max %.3f)\n", generation.mean(), generation.min,
                generation.max);

    // 2. Meshing of every chunk that has all eight neighbours.
    ChunkMesher mesher;
    ChunkMesh mesh;
    MeshTotals culled;
    MeshTotals greedy;
    for (int z = 1; z < side - 1; ++z) {
        for (int x = 1; x < side - 1; ++x) {
            ChunkNeighborhood n;
            for (int dz = -1; dz <= 1; ++dz) {
                for (int dx = -1; dx <= 1; ++dx) {
                    n.at(dx, dz) = chunks[static_cast<std::size_t>((z + dz) * side + (x + dx))].get();
                }
            }
            for (int pass = 0; pass < 2; ++pass) {
                const MeshingMode mode = pass == 0 ? MeshingMode::Culled : MeshingMode::Greedy;
                MeshTotals& totals = pass == 0 ? culled : greedy;
                const auto start = Clock::now();
                mesher.build(n, mesh, mode);
                totals.timeMs.add(millisecondsSince(start));
                totals.vertices += static_cast<std::int64_t>(mesh.vertexCount());
                totals.quads += mesh.quadCount;
            }
        }
    }

    const int meshed = std::max(1, culled.timeMs.samples);
    auto report = [meshed](const char* name, const MeshTotals& t) {
        std::printf("%-19s: %7.3f ms/chunk  %8.0f quads/chunk  %9.0f vertices/chunk\n", name, t.timeMs.mean(),
                    static_cast<double>(t.quads) / meshed, static_cast<double>(t.vertices) / meshed);
    };
    report("Meshing (culled)", culled);
    report("Meshing (greedy)", greedy);
    const double reduction =
        culled.vertices > 0 ? 100.0 * (1.0 - static_cast<double>(greedy.vertices) / static_cast<double>(culled.vertices))
                            : 0.0;
    std::printf("Greedy reduction   : %6.1f%% fewer vertices (%d chunks meshed)\n", reduction, meshed);
    std::printf("Vertex size        : %zu bytes -> %.1f KiB GPU memory per chunk (greedy)\n", sizeof(ChunkVertex),
                static_cast<double>(greedy.vertices) / meshed * sizeof(ChunkVertex) / 1024.0);
    return 0;
}

} // namespace voxelforge
