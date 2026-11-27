#pragma once

#include <cstdint>

namespace voxelforge {

struct BenchmarkOptions {
    std::uint64_t seed = 1337;
    bool quick = false; // smaller region, suitable for CI
};

// Measures terrain generation and meshing (culled vs greedy) on the CPU and prints a
// report to stdout. Needs no window or GPU. Returns the process exit code.
int runBenchmark(const BenchmarkOptions& options);

} // namespace voxelforge
