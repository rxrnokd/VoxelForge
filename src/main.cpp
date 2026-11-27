#include "core/Benchmark.hpp"
#include "core/CommandLine.hpp"
#include "core/Game.hpp"

#include <cstdio>
#include <exception>

int main(int argc, char** argv) {
    using namespace voxelforge;

    const CommandLineResult parsed = parseCommandLine(argc, argv);
    if (!parsed.ok()) {
        std::fprintf(stderr, "voxelforge: %s\n\n%s", parsed.error.c_str(), commandLineUsage());
        return 2;
    }
    if (parsed.options.showHelp) {
        std::printf("%s", commandLineUsage());
        return 0;
    }

    if (parsed.options.benchmark) {
        BenchmarkOptions bench;
        bench.seed = parsed.options.seed.value_or(bench.seed);
        bench.quick = parsed.options.benchmarkQuick;
        return runBenchmark(bench);
    }

    try {
        Game game(parsed.options);
        return game.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "voxelforge: fatal error: %s\n", e.what());
        return 1;
    }
}
