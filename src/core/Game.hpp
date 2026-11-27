#pragma once

#include "core/CommandLine.hpp"
#include "core/DayNightCycle.hpp"
#include "core/Input.hpp"
#include "core/Settings.hpp"
#include "physics/Raycast.hpp"
#include "player/Inventory.hpp"
#include "player/Player.hpp"
#include "render/Renderer.hpp"
#include "render/Sky.hpp"
#include "ui/Hud.hpp"
#include "world/World.hpp"

#include <memory>
#include <optional>

namespace voxelforge {

class Game {
public:
    explicit Game(LaunchOptions options);
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    int run();

private:
    static constexpr float kPhysicsStep = 1.0f / 120.0f;
    static constexpr float kReach = 6.0f;            // blocks
    static constexpr float kRepeatInterval = 0.22f;  // seconds between repeated break/place
    static constexpr int kUploadsPerFrame = 12;       // GPU mesh uploads per frame (hitch control)

    bool initialize();
    void streamWorld(int uploadBudget);
    void update(const FrameInput& input, float dt);
    void updateBlockInteraction(const FrameInput& input, float dt);
    void applyBlockChange(const IVec3& block, BlockType type);
    void render();
    bool saveScreenshot() const;

    LaunchOptions m_options;
    Settings m_settings;
    bool m_windowOpen = false;
    bool m_screenshotMode = false;

    Input m_input;
    std::unique_ptr<World> m_world;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Sky> m_sky;
    DayNightCycle m_dayNight;
    Player m_player;
    Inventory m_inventory;
    Hud m_hud;
    std::optional<RaycastHit> m_target;
    float m_breakCooldown = 0.0f;
    float m_placeCooldown = 0.0f;
    RenderCamera m_camera;
    float m_physicsAccumulator = 0.0f;
    int m_renderDistance = 8;
};

} // namespace voxelforge
