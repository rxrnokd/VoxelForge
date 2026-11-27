#pragma once

#include "core/Math.hpp"
#include "render/Renderer.hpp"
#include "world/Biome.hpp"
#include "world/World.hpp"

#include <cstdint>
#include <optional>

namespace voxelforge {

// Values shown on the F3 screen, gathered by the game each frame.
struct DebugInfo {
    int fps = 0;
    float frameMs = 0.0f;
    Vec3 position;     // feet
    float yaw = 0.0f;  // radians
    float pitch = 0.0f;
    Biome biome = Biome::Plains;
    int hours = 0;
    int minutes = 0;
    bool flying = false;
    bool onGround = false;
    int renderDistance = 0;
    WorldStats world;
    RenderStats render;
    unsigned workerThreads = 0;
    std::uint64_t seed = 0;
    std::optional<IVec3> targetBlock;
    BlockType targetType = BlockType::Air;
};

// F3 overlay. Text is formatted into fixed-size stack buffers: no per-frame allocation.
class DebugOverlay {
public:
    void toggle() { m_visible = !m_visible; }
    void setVisible(bool visible) { m_visible = visible; }
    bool visible() const { return m_visible; }
    void draw(const DebugInfo& info) const;

private:
    bool m_visible = false;
};

} // namespace voxelforge
