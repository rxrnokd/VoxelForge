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
#include "ui/DebugOverlay.hpp"
#include "ui/Hud.hpp"
#include "ui/PauseMenu.hpp"
#include "world/World.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace voxelforge {

// Owns the window and every subsystem, and runs the main loop:
//   input -> fixed-step simulation -> world streaming / GPU uploads -> rendering.
class Game {
public:
    explicit Game(LaunchOptions options);
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    int run();

private:
    static constexpr float kPhysicsStep = 1.0f / 120.0f;
    static constexpr float kReach = 6.0f;              // blocks
    static constexpr float kRepeatInterval = 0.22f;    // seconds between repeated break/place
    static constexpr int kUploadsPerFrame = 12;         // GPU mesh uploads per frame (hitch control)
    static constexpr float kAutosaveInterval = 300.0f; // seconds

    bool initialize();
    bool createWindow();
    void loadOrCreateWorld();
    bool saveWorld();
    void streamWorld(int uploadBudget);

    void update(const FrameInput& input, float dt);
    void updateGameplay(const FrameInput& input, float dt);
    void updateBlockInteraction(const FrameInput& input, float dt);
    void setPaused(bool paused);

    void render(const FrameInput& input);
    DebugInfo collectDebugInfo() const;
    bool saveScreenshot(const std::string& path) const;
    void setStatus(const char* message);

    LaunchOptions m_options;
    Settings m_settings;
    std::string m_savePath;
    bool m_windowOpen = false;
    bool m_screenshotMode = false;
    bool m_savingEnabled = true;
    bool m_quitRequested = false;
    bool m_paused = false;

    Input m_input;
    std::unique_ptr<World> m_world;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Sky> m_sky;
    DayNightCycle m_dayNight;
    Player m_player;
    Inventory m_inventory;
    Hud m_hud;
    DebugOverlay m_debug;
    PauseMenu m_pauseMenu;
    RenderCamera m_camera;

    std::optional<RaycastHit> m_target;
    float m_breakCooldown = 0.0f;
    float m_placeCooldown = 0.0f;
    float m_physicsAccumulator = 0.0f;
    float m_autosaveTimer = 0.0f;
    float m_statusTimer = 0.0f;
    char m_status[96] = {};
    int m_screenshotCounter = 0;
};

} // namespace voxelforge
