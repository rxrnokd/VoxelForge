#include "core/Game.hpp"

#include "io/SaveFile.hpp"

#include "raylib.h"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <random>
#include <utility>

namespace voxelforge {

namespace {
constexpr float kDegToRad = kPi / 180.0f;
constexpr const char* kSettingsPath = "settings.cfg";
static_assert(Inventory::kSlots == kSavedHotbarSlots, "save format stores the whole hotbar");

std::uint64_t randomSeed() {
    std::random_device device;
    return (static_cast<std::uint64_t>(device()) << 32) ^ device();
}
} // namespace

Game::Game(LaunchOptions options) : m_options(std::move(options)) {}

Game::~Game() {
    // Worker threads first, then GPU resources while the OpenGL context still exists.
    m_world.reset();
    m_renderer.reset();
    m_sky.reset();
    if (m_windowOpen) {
        CloseWindow();
    }
}

// ---------------------------------------------------------------------------------------
// Start-up and persistence
// ---------------------------------------------------------------------------------------

bool Game::createWindow() {
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
    SetExitKey(KEY_NULL); // Esc opens the pause menu instead of quitting
    return true;
}

void Game::loadOrCreateWorld() {
    WorldSave save;
    bool loaded = false;
    // Screenshot mode may read an existing save (it never writes one).
    if (!m_options.freshWorld) {
        const SaveStatus status = readWorldSave(m_savePath, save);
        if (status == SaveStatus::Ok) {
            loaded = true;
            std::printf("Loaded %s (%zu modified chunks)\n", m_savePath.c_str(), save.chunks.size());
            if (m_options.seed && *m_options.seed != save.seed) {
                std::printf("Note: --seed is ignored for an existing world (use --new)\n");
            }
        } else if (status != SaveStatus::FileNotFound) {
            std::fprintf(stderr, "voxelforge: could not load %s: %s - starting a new world\n", m_savePath.c_str(),
                         saveStatusMessage(status));
            std::fprintf(stderr, "voxelforge: saving is disabled to protect the existing file\n");
            m_savingEnabled = false; // never overwrite a save we failed to read
        }
    }

    const std::uint64_t seed = loaded ? save.seed : m_options.seed.value_or(m_screenshotMode ? 1337 : randomSeed());
    m_world = std::make_unique<World>(seed);

    if (loaded) {
        for (const SavedChunk& chunk : save.chunks) {
            if (!m_world->addModifiedChunk(chunk)) {
                std::fprintf(stderr, "voxelforge: skipping corrupt chunk %d,%d\n", chunk.pos.x, chunk.pos.z);
            }
        }
        m_player.setPosition(save.player.position);
        m_player.setLook(save.player.yaw, save.player.pitch);
        m_player.setFlying(save.player.flying);
        for (int i = 0; i < Inventory::kSlots; ++i) {
            m_inventory.setSlot(i, save.player.hotbar[static_cast<std::size_t>(i)]);
        }
        m_inventory.select(save.player.selectedSlot);
        m_dayNight.setTimeOfDay(save.timeOfDay);
    } else {
        const Vec3 feet = m_options.position ? *m_options.position - Vec3{0.0f, Player::kEyeHeight, 0.0f}
                                             : m_world->findSpawnPoint(0, 0);
        m_player.setPosition(feet);
        m_player.setLook(0.0f, 0.0f);
        m_dayNight.setTimeOfDay(m_screenshotMode ? 0.12f : 0.05f);
    }

    // Explicit command line values win over the save.
    if (m_options.position && loaded) {
        m_player.setPosition(*m_options.position - Vec3{0.0f, Player::kEyeHeight, 0.0f});
    }
    const float yawDefault = m_screenshotMode ? 30.0f : m_player.yaw() / kDegToRad;
    const float pitchDefault = m_screenshotMode ? -20.0f : m_player.pitch() / kDegToRad;
    m_player.setLook(m_options.yawDegrees.value_or(yawDefault) * kDegToRad,
                     m_options.pitchDegrees.value_or(pitchDefault) * kDegToRad);
    if (m_options.timeOfDay) {
        m_dayNight.setTimeOfDay(*m_options.timeOfDay);
    }
}

bool Game::saveWorld() {
    if (!m_savingEnabled) {
        return false;
    }
    WorldSave save;
    save.seed = m_world->seed();
    save.timeOfDay = m_dayNight.timeOfDay();
    save.player.position = m_player.position();
    save.player.yaw = m_player.yaw();
    save.player.pitch = m_player.pitch();
    save.player.flying = m_player.flying();
    save.player.selectedSlot = m_inventory.selectedSlot();
    for (int i = 0; i < Inventory::kSlots; ++i) {
        save.player.hotbar[static_cast<std::size_t>(i)] = m_inventory.slot(i);
    }
    save.chunks = m_world->modifiedChunks();
    const SaveStatus status = writeWorldSave(m_savePath, save);
    if (status != SaveStatus::Ok) {
        std::fprintf(stderr, "voxelforge: saving failed: %s\n", saveStatusMessage(status));
        setStatus("Saving failed!");
        return false;
    }
    char message[96];
    std::snprintf(message, sizeof(message), "World saved (%zu modified chunks)", save.chunks.size());
    setStatus(message);
    return true;
}

bool Game::initialize() {
    m_screenshotMode = !m_options.screenshotPath.empty();
    m_savingEnabled = !m_screenshotMode && !m_options.noSave;
    if (!m_screenshotMode) {
        m_settings.loadFromFile(kSettingsPath);
    }
    if (m_options.renderDistance) {
        m_settings.renderDistance = *m_options.renderDistance;
    }
    m_savePath = "saves/" + m_options.worldName + ".vxf";
    m_dayNight.setDayLength(m_settings.dayLengthSeconds);

    if (!createWindow()) {
        return false;
    }
    m_renderer = std::make_unique<Renderer>();
    m_sky = std::make_unique<Sky>();
    if (!m_renderer->init() || !m_sky->init()) {
        return false;
    }

    loadOrCreateWorld();

    // Screenshots need the whole view distance; normal play starts once the area around
    // the player is ready and keeps streaming in the background.
    BeginDrawing();
    ClearBackground(Color{20, 22, 30, 255});
    DrawText("Generating world...", 40, 40, 30, RAYWHITE);
    EndDrawing();
    const int radius = m_screenshotMode ? m_settings.renderDistance : std::min(m_settings.renderDistance, 3);
    if (!m_world->waitForArea(m_player.eyePosition(), m_settings.renderDistance, radius, 120.0)) {
        std::fprintf(stderr, "voxelforge: warning: world did not finish loading in time\n");
    }
    streamWorld(std::numeric_limits<int>::max());

    m_debug.setVisible(m_options.showDebug);
    if (m_options.showMenu) {
        m_paused = true;
    } else if (!m_screenshotMode) {
        m_input.setCursorCaptured(true);
    }
    return true;
}

void Game::streamWorld(int uploadBudget) {
    m_world->update(m_player.eyePosition(), m_settings.renderDistance);
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

void Game::setStatus(const char* message) {
    std::snprintf(m_status, sizeof(m_status), "%s", message);
    m_statusTimer = 3.0f;
}

// ---------------------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------------------

void Game::setPaused(bool paused) {
    if (paused == m_paused) {
        return;
    }
    m_paused = paused;
    m_input.setCursorCaptured(!paused);
    if (!paused && !m_screenshotMode && !m_options.noSave) {
        m_settings.saveToFile(kSettingsPath); // persist changes made in the menu
    }
}

void Game::update(const FrameInput& input, float dt) {
    m_statusTimer = std::max(0.0f, m_statusTimer - dt);
    if (input.toggleDebug) {
        m_debug.toggle();
    }
    if (input.takeScreenshot) {
        char name[64];
        std::snprintf(name, sizeof(name), "screenshot_%03d.png", m_screenshotCounter++);
        if (saveScreenshot(name)) {
            setStatus(name);
        }
    }
    if (input.togglePause && !m_screenshotMode) {
        setPaused(!m_paused);
    }
    if (!m_paused) {
        updateGameplay(input, dt);
    }

    if (m_savingEnabled) {
        m_autosaveTimer += dt;
        if (m_autosaveTimer >= kAutosaveInterval) {
            m_autosaveTimer = 0.0f;
            saveWorld();
        }
    }
    m_dayNight.setDayLength(m_settings.dayLengthSeconds);
    streamWorld(kUploadsPerFrame);
}

void Game::updateGameplay(const FrameInput& input, float dt) {
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
    if (!m_screenshotMode) {
        m_dayNight.advance(input.fastTime ? dt * 60.0f : dt);
    }

    // Fixed time step physics; rendering interpolates between the last two states. The
    // simulation waits until block data under the player exists, and screenshots taken
    // from an explicit --pos keep the camera frozen.
    const Vec3 feet = m_player.position();
    const bool groundLoaded = m_world->isChunkLoaded(ChunkPos::fromWorld(fastFloor(feet.x), fastFloor(feet.z)));
    if (groundLoaded && !(m_screenshotMode && m_options.position)) {
        m_physicsAccumulator = std::min(m_physicsAccumulator + dt, 0.25f);
        while (m_physicsAccumulator >= kPhysicsStep) {
            m_player.step(*m_world, input.move, kPhysicsStep);
            m_physicsAccumulator -= kPhysicsStep;
        }
        if (m_player.position().y < -64.0f) { // fell out of the world
            m_player.setPosition(m_world->findSpawnPoint(fastFloor(feet.x), fastFloor(feet.z)));
        }
    }
    updateBlockInteraction(input, dt);
}

void Game::updateBlockInteraction(const FrameInput& input, float dt) {
    m_target = raycastBlocks(*m_world, m_player.eyePosition(), m_player.forward(), kReach);
    m_breakCooldown = input.breakHeld ? std::max(0.0f, m_breakCooldown - dt) : 0.0f;
    m_placeCooldown = input.placeHeld ? std::max(0.0f, m_placeCooldown - dt) : 0.0f;
    if (!m_target) {
        return;
    }

    if (input.pickPressed) {
        m_inventory.pick(m_target->type);
        m_hud.notifySelectionChanged();
    }

    if ((input.breakPressed || input.breakHeld) && m_breakCooldown <= 0.0f) {
        if (m_target->type != BlockType::Bedrock) {
            m_world->setBlock(m_target->block.x, m_target->block.y, m_target->block.z, BlockType::Air);
        }
        m_breakCooldown = kRepeatInterval;
        return;
    }

    if ((input.placePressed || input.placeHeld) && m_placeCooldown <= 0.0f && m_target->normal != IVec3{}) {
        const IVec3 target = m_target->placementTarget();
        const BlockType current = m_world->getBlock(target.x, target.y, target.z);
        const BlockType block = m_inventory.selectedBlock();
        const bool replaceable = current == BlockType::Air || current == BlockType::Water;
        const bool blocksPlayer =
            isSolid(block) && AABB::fromBlock(target.x, target.y, target.z).intersects(m_player.bounds());
        if (replaceable && !blocksPlayer) {
            m_world->setBlock(target.x, target.y, target.z, block);
        }
        m_placeCooldown = kRepeatInterval;
    }
}

// ---------------------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------------------

DebugInfo Game::collectDebugInfo() const {
    DebugInfo info;
    info.fps = GetFPS();
    info.frameMs = GetFrameTime() * 1000.0f;
    info.position = m_player.position();
    info.yaw = m_player.yaw();
    info.pitch = m_player.pitch();
    info.biome = m_world->generator().biomeAt(fastFloor(info.position.x), fastFloor(info.position.z));
    info.hours = m_dayNight.hours();
    info.minutes = m_dayNight.minutes();
    info.flying = m_player.flying();
    info.onGround = m_player.onGround();
    info.renderDistance = m_settings.renderDistance;
    info.world = m_world->stats();
    info.render = m_renderer->stats();
    info.workerThreads = m_world->workerThreads();
    info.seed = m_world->seed();
    if (m_target) {
        info.targetBlock = m_target->block;
        info.targetType = m_target->type;
    }
    return info;
}

void Game::render(const FrameInput& input) {
    const float alpha = m_physicsAccumulator / kPhysicsStep;
    m_camera.position = m_player.interpolatedPosition(alpha) + Vec3{0.0f, Player::kEyeHeight, 0.0f};
    m_camera.forward = m_player.forward();
    m_camera.fovYRadians = m_settings.fovDegrees * kDegToRad;
    m_camera.aspect = static_cast<float>(GetScreenWidth()) / static_cast<float>(std::max(1, GetScreenHeight()));
    m_camera.farPlane = static_cast<float>(m_settings.renderDistance * kChunkSize) * 1.5f + 64.0f;
    m_camera.maxChunkDistance = static_cast<float>((m_settings.renderDistance + 1) * kChunkSize);

    const IVec3 eye{fastFloor(m_camera.position.x), fastFloor(m_camera.position.y), fastFloor(m_camera.position.z)};
    const bool underwater = m_world->getBlock(eye.x, eye.y, eye.z) == BlockType::Water;
    const SkyState sky = m_dayNight.sky();
    const SceneLighting lighting = makeSceneLighting(sky, static_cast<float>(m_settings.renderDistance * kChunkSize),
                                                     static_cast<float>(GetTime()), underwater);

    BeginDrawing();
    ClearBackground(BLACK);
    m_sky->draw(m_camera, sky);
    m_renderer->drawWorld(m_camera, lighting);
    if (m_target && !m_paused) {
        m_renderer->drawBlockOutline(m_camera, m_target->block);
    }
    if (underwater) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{20, 60, 140, 90});
    }

    m_hud.draw(m_renderer->atlasTexture(), m_inventory, GetScreenWidth(), GetScreenHeight());
    if (m_debug.visible()) {
        m_debug.draw(collectDebugInfo()); // stats walk every chunk: only when shown
    }

    if (m_paused) {
        const PauseMenu::Action action = m_pauseMenu.draw(m_settings, input, GetScreenWidth(), GetScreenHeight(),
                                                          m_statusTimer > 0.0f ? m_status : "", m_savingEnabled);
        switch (action) {
        case PauseMenu::Action::Resume: setPaused(false); break;
        case PauseMenu::Action::Save: saveWorld(); break;
        case PauseMenu::Action::SaveAndQuit: m_quitRequested = true; break;
        case PauseMenu::Action::None: break;
        }
    } else if (m_statusTimer > 0.0f) {
        const int w = MeasureText(m_status, 20);
        DrawText(m_status, (GetScreenWidth() - w) / 2, 40, 20, Color{200, 255, 200, 230});
    }
    EndDrawing();
}

bool Game::saveScreenshot(const std::string& path) const {
    Image image = LoadImageFromScreen();
    const bool ok = ExportImage(image, path.c_str());
    UnloadImage(image);
    if (ok) {
        std::printf("Saved screenshot to %s\n", path.c_str());
    }
    return ok;
}

int Game::run() {
    if (!initialize()) {
        return 1;
    }
    int frame = 0;
    while (!WindowShouldClose() && !m_quitRequested) {
        const FrameInput input = m_input.poll();
        update(input, std::min(GetFrameTime(), 0.1f));
        render(input);
        if (m_screenshotMode && ++frame >= m_options.screenshotFrames) {
            return saveScreenshot(m_options.screenshotPath) ? 0 : 1;
        }
    }
    if (m_savingEnabled) {
        saveWorld();
    }
    if (!m_screenshotMode && !m_options.noSave) {
        m_settings.saveToFile(kSettingsPath);
    }
    return 0;
}

} // namespace voxelforge
