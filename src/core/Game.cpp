#include "core/Game.hpp"

#include "physics/Collision.hpp"

#include "raylib.h"

#include <algorithm>
#include <cstdio>
#include <limits>
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

    // Screenshots need the whole view distance; normal play starts once the area
    // around the player is ready and keeps streaming in the background.
    const int initialRadius = m_screenshotMode ? m_renderDistance : std::min(m_renderDistance, 3);
    if (!m_world->waitForArea(m_player.eyePosition(), m_renderDistance, initialRadius, 120.0)) {
        std::fprintf(stderr, "voxelforge: warning: world did not finish loading in time\n");
    }
    streamWorld(std::numeric_limits<int>::max());

    if (!m_screenshotMode) {
        m_input.setCursorCaptured(true);
    }
    return true;
}

void Game::streamWorld(int uploadBudget) {
    m_world->update(m_player.eyePosition(), m_renderDistance);
    // Releases first: a chunk may unload and reload within one frame.
    ChunkPos unloaded;
    while (m_world->popUnloadedChunk(unloaded)) {
        m_renderer->removeChunk(unloaded);
    }
    MeshUpdate update;
    for (int uploaded = 0; uploaded < uploadBudget && m_world->popMeshUpdate(update); ++uploaded) {
        m_renderer->setChunkMesh(update.pos, *update.mesh);
        m_world->recycleMesh(std::move(update.mesh));
    }
}

void Game::update(const FrameInput& input, float dt) {
    if (input.toggleFly) {
        m_player.setFlying(!m_player.flying());
    }

    const float sensitivity = m_settings.mouseSensitivity * kDegToRad;
    const float invert = m_settings.invertMouseY ? -1.0f : 1.0f;
    m_player.addLook(input.mouseDeltaX * sensitivity, -input.mouseDeltaY * sensitivity * invert);

    if (input.hotbarSlot >= 0) {
        m_inventory.select(input.hotbarSlot);
        m_hud.notifySelectionChanged();
    }
    if (input.wheel != 0.0f) {
        m_inventory.scroll(input.wheel);
        m_hud.notifySelectionChanged();
    }
    m_hud.update(dt);

    // Fixed time step physics; rendering interpolates between the last two states.
    // Screenshots taken from an explicit --pos keep the camera frozen in place.
    // The simulation waits until block data under the player exists.
    const Vec3 feet = m_player.position();
    const bool groundLoaded = m_world->isChunkLoaded(ChunkPos::fromWorld(fastFloor(feet.x), fastFloor(feet.z)));
    if (!(m_screenshotMode && m_options.position) && groundLoaded) {
        m_physicsAccumulator = std::min(m_physicsAccumulator + dt, 0.25f);
        while (m_physicsAccumulator >= kPhysicsStep) {
            m_player.step(*m_world, input.move, kPhysicsStep);
            m_physicsAccumulator -= kPhysicsStep;
        }
    }
    updateBlockInteraction(input, dt);
    streamWorld(kUploadsPerFrame);
}

void Game::updateBlockInteraction(const FrameInput& input, float dt) {
    m_target = raycastBlocks(*m_world, m_player.eyePosition(), m_player.forward(), kReach);
    m_breakCooldown = std::max(0.0f, m_breakCooldown - dt);
    m_placeCooldown = std::max(0.0f, m_placeCooldown - dt);
    if (!input.breakHeld) m_breakCooldown = 0.0f;
    if (!input.placeHeld) m_placeCooldown = 0.0f;
    if (!m_target) {
        return;
    }

    if (input.pickPressed) {
        m_inventory.pick(m_target->type);
        m_hud.notifySelectionChanged();
    }

    if ((input.breakPressed || input.breakHeld) && m_breakCooldown <= 0.0f) {
        if (m_target->type != BlockType::Bedrock) {
            applyBlockChange(m_target->block, BlockType::Air);
        }
        m_breakCooldown = kRepeatInterval;
        return;
    }

    if ((input.placePressed || input.placeHeld) && m_placeCooldown <= 0.0f && m_target->normal != IVec3{}) {
        const IVec3 target = m_target->placementTarget();
        const BlockType current = m_world->getBlock(target.x, target.y, target.z);
        const BlockType block = m_inventory.selectedBlock();
        const bool replaceable = current == BlockType::Air || current == BlockType::Water;
        const bool blocksPlayer = isSolid(block) && AABB::fromBlock(target.x, target.y, target.z).intersects(m_player.bounds());
        if (replaceable && !blocksPlayer && target.y >= 0 && target.y < kChunkHeight) {
            applyBlockChange(target, block);
        }
        m_placeCooldown = kRepeatInterval;
    }
}

void Game::applyBlockChange(const IVec3& block, BlockType type) {
    // The world re-meshes affected chunks on the worker pool at high priority.
    m_world->setBlock(block.x, block.y, block.z, type);
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
    if (m_target) {
        m_renderer->drawBlockOutline(m_camera, m_target->block);
    }
    m_hud.draw(m_renderer->atlasTexture(), m_inventory, GetScreenWidth(), GetScreenHeight());
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
