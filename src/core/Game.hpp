#pragma once

#include "core/CommandLine.hpp"
#include "core/Input.hpp"
#include "core/Settings.hpp"
#include "player/Player.hpp"
#include "render/Renderer.hpp"
#include "world/World.hpp"

#include <memory>

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

    bool initialize();
    void update(const FrameInput& input, float dt);
    void render();
    bool saveScreenshot() const;

    LaunchOptions m_options;
    Settings m_settings;
    bool m_windowOpen = false;
    bool m_screenshotMode = false;

    Input m_input;
    std::unique_ptr<World> m_world;
    std::unique_ptr<Renderer> m_renderer;
    Player m_player;
    RenderCamera m_camera;
    float m_physicsAccumulator = 0.0f;
    int m_renderDistance = 8;
};

} // namespace voxelforge
