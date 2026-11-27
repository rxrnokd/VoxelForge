#include "core/Game.hpp"

#include "raylib.h"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace voxelforge {

namespace {
constexpr float kDegToRad = kPi / 180.0f;
} // namespace

Game::Game(LaunchOptions options) : m_options(std::move(options)) {}

Game::~Game() {
    // GPU resources must be released while the OpenGL context still exists.
    m_renderer.reset();
    if (m_windowOpen) {
        CloseWindow();
    }
}

bool Game::initialize() {
    m_screenshotMode = !m_options.screenshotPath.empty();
    if (!m_screenshotMode) {
        m_settings.loadFromFile("settings.cfg");
    }
    m_renderDistance = m_options.renderDistance.value_or(m_settings.renderDistance);

    SetTraceLogLevel(LOG_WARNING);
    unsigned int flags = static_cast<unsigned int>(FLAG_MSAA_4X_HINT);
    if (!m_screenshotMode) {
        flags |= static_cast<unsigned int>(FLAG_WINDOW_RESIZABLE);
        if (m_settings.vsync) {
            flags |= static_cast<unsigned int>(FLAG_VSYNC_HINT);
        }
    }
    SetConfigFlags(flags);
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

    // Spawn: explicit eye position from the command line, otherwise dry land near 0,0.
    const Vec3 feet = m_options.position
                          ? *m_options.position - Vec3{0.0f, Player::kEyeHeight, 0.0f}
                          : m_world->findSpawnPoint(0, 0);
    m_player.setPosition(feet);
    m_player.setLook(m_options.yawDegrees.value_or(30.0f) * kDegToRad,
                     m_options.pitchDegrees.value_or(m_screenshotMode ? -20.0f : 0.0f) * kDegToRad);

    const ChunkPos center = ChunkPos::fromWorld(fastFloor(feet.x), fastFloor(feet.z));
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

    if (!m_screenshotMode) {
        m_input.setCursorCaptured(true);
    }
    return true;
}

void Game::update(const FrameInput& input, float dt) {
    if (input.toggleFly) {
        m_player.setFlying(!m_player.flying());
    }

    const float sensitivity = m_settings.mouseSensitivity * kDegToRad;
    const float invert = m_settings.invertMouseY ? -1.0f : 1.0f;
    m_player.addLook(input.mouseDeltaX * sensitivity, -input.mouseDeltaY * sensitivity * invert);

    // Fixed time step physics; rendering interpolates between the last two states.
    // Screenshots taken from an explicit --pos keep the camera frozen in place.
    if (!(m_screenshotMode && m_options.position)) {
        m_physicsAccumulator = std::min(m_physicsAccumulator + dt, 0.25f);
        while (m_physicsAccumulator >= kPhysicsStep) {
            m_player.step(*m_world, input.move, kPhysicsStep);
            m_physicsAccumulator -= kPhysicsStep;
        }
    }
}

void Game::render() {
    const float alpha = m_physicsAccumulator / kPhysicsStep;
    m_camera.position = m_player.interpolatedPosition(alpha) + Vec3{0.0f, Player::kEyeHeight, 0.0f};
    m_camera.forward = m_player.forward();
    m_camera.fovYRadians = m_settings.fovDegrees * kDegToRad;
    m_camera.aspect = static_cast<float>(GetScreenWidth()) / static_cast<float>(std::max(1, GetScreenHeight()));
    m_camera.farPlane = static_cast<float>(m_renderDistance * kChunkSize) * 1.5f + 64.0f;

    const float fogEnd = static_cast<float>(m_renderDistance * kChunkSize) - 8.0f;
    SceneLighting lighting;
    lighting.fogEnd = fogEnd;
    lighting.fogStart = fogEnd * 0.55f;
    lighting.timeSeconds = static_cast<float>(GetTime());

    BeginDrawing();
    ClearBackground(Color{158, 194, 242, 255});
    m_renderer->drawWorld(m_camera, lighting);
    // Crosshair
    const int cx = GetScreenWidth() / 2;
    const int cy = GetScreenHeight() / 2;
    DrawRectangle(cx - 9, cy - 1, 18, 2, Color{255, 255, 255, 200});
    DrawRectangle(cx - 1, cy - 9, 2, 18, Color{255, 255, 255, 200});
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
    int frame = 0;
    while (!WindowShouldClose()) {
        const FrameInput input = m_input.poll();
        update(input, std::min(GetFrameTime(), 0.1f));
        render();
        if (m_screenshotMode && ++frame >= m_options.screenshotFrames) {
            return saveScreenshot() ? 0 : 1;
        }
    }
    return 0;
}

} // namespace voxelforge
