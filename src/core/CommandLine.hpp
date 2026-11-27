#pragma once

#include "core/Math.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace voxelforge {

// Options parsed from the command line. Unset optionals fall back to settings/save data.
struct LaunchOptions {
    std::optional<std::uint64_t> seed;
    std::string worldName = "world";
    std::optional<int> renderDistance;
    std::optional<float> timeOfDay;      // [0, 1): 0 sunrise, 0.25 noon, 0.5 sunset, 0.75 midnight
    std::optional<Vec3> position;        // player eye position override
    std::optional<float> yawDegrees;
    std::optional<float> pitchDegrees;

    std::string screenshotPath;          // non-empty: render a few frames, save PNG, exit
    int screenshotFrames = 10;
    bool benchmark = false;              // print generation/meshing statistics and exit
    bool benchmarkQuick = false;         // smaller benchmark suitable for CI

    int windowWidth = 1280;
    int windowHeight = 720;
    bool freshWorld = false;             // ignore an existing save file
    bool noSave = false;                 // never write save/settings files
    bool showHelp = false;
};

struct CommandLineResult {
    LaunchOptions options;
    std::string error; // empty when parsing succeeded
    bool ok() const { return error.empty(); }
};

CommandLineResult parseCommandLine(int argc, const char* const* argv);
const char* commandLineUsage();

} // namespace voxelforge
