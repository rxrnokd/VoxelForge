#include "core/Settings.hpp"

#include "core/Math.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace voxelforge {
namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

bool toFloat(std::string_view text, float& out) {
    if (text.empty() || text.size() > 31) return false;
    char buffer[32];
    text.copy(buffer, text.size());
    buffer[text.size()] = '\0';
    char* end = nullptr;
    const float v = std::strtof(buffer, &end);
    if (end != buffer + text.size() || !std::isfinite(v)) return false;
    out = v;
    return true;
}

bool toBool(std::string_view text, bool& out) {
    if (text == "true" || text == "1" || text == "on") { out = true; return true; }
    if (text == "false" || text == "0" || text == "off") { out = false; return true; }
    return false;
}

} // namespace

void Settings::clampToLimits() {
    mouseSensitivity = clampf(mouseSensitivity, kMinSensitivity, kMaxSensitivity);
    renderDistance = std::clamp(renderDistance, kMinRenderDistance, kMaxRenderDistance);
    fovDegrees = clampf(fovDegrees, 40.0f, 110.0f);
    dayLengthSeconds = clampf(dayLengthSeconds, 30.0f, 7200.0f);
}

std::string Settings::serialize() const {
    char buffer[512];
    std::snprintf(buffer, sizeof(buffer),
                  "# VoxelForge settings\n"
                  "mouse_sensitivity = %.4f\n"
                  "render_distance = %d\n"
                  "fov = %.1f\n"
                  "invert_mouse_y = %s\n"
                  "vsync = %s\n"
                  "day_length = %.1f\n",
                  static_cast<double>(mouseSensitivity), renderDistance, static_cast<double>(fovDegrees),
                  invertMouseY ? "true" : "false", vsync ? "true" : "false", static_cast<double>(dayLengthSeconds));
    return buffer;
}

Settings Settings::parse(std::string_view text) {
    Settings s;
    while (!text.empty()) {
        const std::size_t newline = text.find('\n');
        std::string_view line = trim(text.substr(0, newline));
        text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);
        if (line.empty() || line.front() == '#') continue;
        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        const std::string_view key = trim(line.substr(0, eq));
        const std::string_view value = trim(line.substr(eq + 1));

        float f = 0.0f;
        bool b = false;
        if (key == "mouse_sensitivity" && toFloat(value, f)) s.mouseSensitivity = f;
        else if (key == "render_distance" && toFloat(value, f)) s.renderDistance = static_cast<int>(f);
        else if (key == "fov" && toFloat(value, f)) s.fovDegrees = f;
        else if (key == "invert_mouse_y" && toBool(value, b)) s.invertMouseY = b;
        else if (key == "vsync" && toBool(value, b)) s.vsync = b;
        else if (key == "day_length" && toFloat(value, f)) s.dayLengthSeconds = f;
    }
    s.clampToLimits();
    return s;
}

bool Settings::loadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    std::stringstream buffer;
    buffer << in.rdbuf();
    *this = parse(buffer.str());
    return true;
}

bool Settings::saveToFile(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << serialize();
    return static_cast<bool>(out);
}

} // namespace voxelforge
