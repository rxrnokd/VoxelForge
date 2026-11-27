#pragma once

#include "core/CommandLine.hpp"
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
    bool initialize();
    void render();
    bool saveScreenshot() const;

    LaunchOptions m_options;
    bool m_windowOpen = false;
    std::unique_ptr<World> m_world;
    std::unique_ptr<Renderer> m_renderer;
    RenderCamera m_camera;
    int m_renderDistance = 8;
};

} // namespace voxelforge
