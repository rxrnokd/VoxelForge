#include "core/Game.hpp"

#include "raylib.h"

#include <cstdio>
#include <utility>

namespace voxelforge {

Game::Game(LaunchOptions options) : m_options(std::move(options)) {}

Game::~Game() {
    // GPU resources must be released while the OpenGL context still exists.
    m_renderer.reset();
    if (m_windowOpen) {
        CloseWindow();
    }
}

bool Game::initialize() {
    const bool screenshotMode = !m_options.screenshotPath.empty();
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(screenshotMode ? 0u : (FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT));
    InitWindow(m_options.windowWidth, m_options.windowHeight, "VoxelForge");
    if (!IsWindowReady()) {
        std::fprintf(stderr, "voxelforge: could not create a window / OpenGL 3.3 context\n");
        return false;
    }
    m_windowOpen = true;
    SetExitKey(KEY_NULL);

    m_renderer = std::make_unique<Renderer>();
    if (!m_renderer->init()) {
        return false;
    }

    m_world = std::make_unique<World>(m_options.seed.value_or(1337));
    m_renderDistance = m_options.renderDistance.value_or(8);

    const Vec3 start = m_options.position.value_or(
        Vec3{8.5f, static_cast<float>(m_world->generator().surfaceHeight(8, 8)) + 24.0f, 8.5f});
    const float yaw = m_options.yawDegrees.value_or(30.0f) * kPi / 180.0f;
    const float pitch = m_options.pitchDegrees.value_or(-25.0f) * kPi / 180.0f;
    m_camera.position = start;
    m_camera.forward = directionFromYawPitch(yaw, pitch);

    const ChunkPos center = ChunkPos::fromWorld(fastFloor(start.x), fastFloor(start.z));
    m_world->generateArea(center, m_renderDistance);
    ChunkMesher mesher;
    ChunkMesh mesh;
    for (int dz = -m_renderDistance; dz <= m_renderDistance; ++dz) {
        for (int dx = -m_renderDistance; dx <= m_renderDistance; ++dx) {
            const ChunkPos pos{center.x + dx, center.z + dz};
            if (m_world->buildMesh(pos, mesher, mesh)) {
                m_renderer->setChunkMesh(pos, mesh);
            }
        }
    }
    return true;
}

void Game::render() {
    const float fogEnd = static_cast<float>(m_renderDistance * kChunkSize) - 8.0f;
    SceneLighting lighting;
    lighting.fogEnd = fogEnd;
    lighting.fogStart = fogEnd * 0.55f;
    lighting.timeSeconds = static_cast<float>(GetTime());

    m_camera.aspect = static_cast<float>(GetScreenWidth()) / static_cast<float>(GetScreenHeight());
    m_camera.farPlane = static_cast<float>(m_renderDistance * kChunkSize) * 1.5f + 64.0f;

    BeginDrawing();
    ClearBackground(Color{158, 194, 242, 255});
    m_renderer->drawWorld(m_camera, lighting);
    DrawFPS(10, 10);
    EndDrawing();
}

bool Game::saveScreenshot() const {
    Image image = LoadImageFromScreen();
    const bool ok = ExportImage(image, m_options.screenshotPath.c_str());
    UnloadImage(image);
    if (ok) {
        std::printf("Saved screenshot to %s\n", m_options.screenshotPath.c_str());
    }
    return ok;
}

int Game::run() {
    if (!initialize()) {
        return 1;
    }
    const bool screenshotMode = !m_options.screenshotPath.empty();
    int frame = 0;
    while (!WindowShouldClose()) {
        render();
        if (screenshotMode && ++frame >= m_options.screenshotFrames) {
            return saveScreenshot() ? 0 : 1;
        }
    }
    return 0;
}

} // namespace voxelforge
