#pragma once

#include <string>
#include <string_view>

namespace voxelforge {

// User preferences persisted as a small "key = value" text file.
struct Settings {
    static constexpr int kMinRenderDistance = 2;
    static constexpr int kMaxRenderDistance = 32;
    static constexpr float kMinSensitivity = 0.01f;
    static constexpr float kMaxSensitivity = 1.0f;

    float mouseSensitivity = 0.12f; // degrees of rotation per pixel of mouse movement
    int renderDistance = 10;        // chunks
    float fovDegrees = 75.0f;
    bool invertMouseY = false;
    bool vsync = true;
    float dayLengthSeconds = 600.0f; // one full day/night cycle

    // Forces every field into its valid range.
    void clampToLimits();

    std::string serialize() const;
    // Unknown keys and malformed lines are ignored; missing keys keep their defaults.
    static Settings parse(std::string_view text);

    bool loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path) const;
};

} // namespace voxelforge
