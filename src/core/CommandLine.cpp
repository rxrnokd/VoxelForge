#include "core/CommandLine.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string_view>
#include <system_error>

namespace voxelforge {
namespace {

bool parseInt(std::string_view text, long long& out) {
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parseUnsigned(std::string_view text, std::uint64_t& out) {
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc{} && result.ptr == end;
}

// std::from_chars for floating point is missing from older Apple libc++, so use strtof
// on a bounded copy instead.
bool parseFloat(std::string_view text, float& out) {
    if (text.empty() || text.size() > 63) {
        return false;
    }
    char buffer[64];
    text.copy(buffer, text.size());
    buffer[text.size()] = '\0';
    char* end = nullptr;
    out = std::strtof(buffer, &end);
    return end == buffer + text.size() && std::isfinite(out);
}

bool parseVec3(std::string_view text, Vec3& out) {
    float values[3];
    for (int i = 0; i < 3; ++i) {
        const std::size_t comma = text.find(',');
        const std::string_view part = (i < 2) ? text.substr(0, comma) : text;
        if ((i < 2 && comma == std::string_view::npos) || !parseFloat(part, values[i])) {
            return false;
        }
        if (i < 2) {
            text.remove_prefix(comma + 1);
        }
    }
    out = {values[0], values[1], values[2]};
    return true;
}

} // namespace

CommandLineResult parseCommandLine(int argc, const char* const* argv) {
    CommandLineResult result;
    LaunchOptions& o = result.options;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto needValue = [&](std::string_view& value) -> bool {
            if (i + 1 >= argc) {
                result.error = std::string("missing value for ") + std::string(arg);
                return false;
            }
            value = argv[++i];
            return true;
        };
        auto fail = [&](const char* what) {
            result.error = std::string("invalid value for ") + std::string(arg) + ": " + what;
        };

        std::string_view value;
        long long integer = 0;
        float real = 0.0f;

        if (arg == "--help" || arg == "-h") {
            o.showHelp = true;
        } else if (arg == "--seed") {
            std::uint64_t seed = 0;
            if (!needValue(value)) return result;
            if (!parseUnsigned(value, seed)) { fail("expected an unsigned integer"); return result; }
            o.seed = seed;
        } else if (arg == "--world") {
            if (!needValue(value)) return result;
            if (value.empty() || value.find_first_of("/\\.") != std::string_view::npos) {
                fail("world names may not be empty or contain path separators");
                return result;
            }
            o.worldName = std::string(value);
        } else if (arg == "--render-distance") {
            if (!needValue(value)) return result;
            if (!parseInt(value, integer) || integer < 2 || integer > 32) { fail("expected 2..32"); return result; }
            o.renderDistance = static_cast<int>(integer);
        } else if (arg == "--time") {
            if (!needValue(value)) return result;
            if (!parseFloat(value, real) || real < 0.0f || real >= 1.0f) { fail("expected 0 <= t < 1"); return result; }
            o.timeOfDay = real;
        } else if (arg == "--pos") {
            Vec3 pos;
            if (!needValue(value)) return result;
            if (!parseVec3(value, pos)) { fail("expected X,Y,Z"); return result; }
            o.position = pos;
        } else if (arg == "--yaw") {
            if (!needValue(value)) return result;
            if (!parseFloat(value, real)) { fail("expected degrees"); return result; }
            o.yawDegrees = real;
        } else if (arg == "--pitch") {
            if (!needValue(value)) return result;
            if (!parseFloat(value, real) || real < -89.9f || real > 89.9f) { fail("expected -89.9..89.9"); return result; }
            o.pitchDegrees = real;
        } else if (arg == "--screenshot") {
            if (!needValue(value)) return result;
            if (value.empty()) { fail("expected a file name"); return result; }
            o.screenshotPath = std::string(value);
        } else if (arg == "--frames") {
            if (!needValue(value)) return result;
            if (!parseInt(value, integer) || integer < 1 || integer > 10000) { fail("expected 1..10000"); return result; }
            o.screenshotFrames = static_cast<int>(integer);
        } else if (arg == "--benchmark") {
            o.benchmark = true;
        } else if (arg == "--quick") {
            o.benchmarkQuick = true;
        } else if (arg == "--width" || arg == "--height") {
            if (!needValue(value)) return result;
            if (!parseInt(value, integer) || integer < 64 || integer > 8192) { fail("expected 64..8192"); return result; }
            (arg == "--width" ? o.windowWidth : o.windowHeight) = static_cast<int>(integer);
        } else if (arg == "--new") {
            o.freshWorld = true;
        } else if (arg == "--no-save") {
            o.noSave = true;
        } else if (arg == "--debug") {
            o.showDebug = true;
        } else if (arg == "--menu") {
            o.showMenu = true;
        } else {
            result.error = std::string("unknown option ") + std::string(arg);
            return result;
        }
    }
    return result;
}

const char* commandLineUsage() {
    return "Usage: voxelforge [options]\n"
           "  --seed N              world seed (new worlds only)\n"
           "  --world NAME          save slot name (default: world)\n"
           "  --new                 start a fresh world, ignoring an existing save\n"
           "  --no-save             never write save or settings files\n"
           "  --render-distance N   view distance in chunks (2..32)\n"
           "  --time T              time of day in [0,1): 0 sunrise, 0.25 noon, 0.5 sunset\n"
           "  --pos X,Y,Z           start position (eye)\n"
           "  --yaw DEG --pitch DEG start orientation\n"
           "  --width W --height H  window size\n"
           "  --screenshot FILE     render --frames frames, save a PNG and exit\n"
           "  --frames N            frames to render in screenshot mode (default 10)\n"
           "  --benchmark [--quick] print chunk generation / meshing statistics and exit\n"
           "  --debug               start with the F3 debug overlay visible\n"
           "  --menu                start with the pause menu open\n"
           "  --help                show this help\n";
}

} // namespace voxelforge
