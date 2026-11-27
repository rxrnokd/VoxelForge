// Verifies the "no heap allocation per frame" rule for the simulation side of a frame:
// world streaming in steady state, physics, raycasting and the day/night model.
// Global operator new is replaced with a counting version that only counts on the
// thread that enabled it, so worker thread allocations are ignored.

#include "doctest.h"

#include "core/DayNightCycle.hpp"
#include "physics/Raycast.hpp"
#include "player/Player.hpp"
#include "render/Frustum.hpp"
#include "world/World.hpp"

#include <atomic>
#include <cstdlib>
#include <new>
#include <vector>

namespace {
thread_local bool g_counting = false;
std::atomic<long> g_allocations{0};

void* countedAlloc(std::size_t size) {
    if (g_counting) {
        g_allocations.fetch_add(1, std::memory_order_relaxed);
    }
    void* p = std::malloc(size != 0 ? size : 1);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}
} // namespace

void* operator new(std::size_t size) { return countedAlloc(size); }
void* operator new[](std::size_t size) { return countedAlloc(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return countedAlloc(size);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return countedAlloc(size);
    } catch (...) {
        return nullptr;
    }
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }

using namespace voxelforge;

TEST_CASE("allocations: the counter detects heap allocations") {
    volatile std::size_t size = 32;
    g_allocations = 0;
    g_counting = true;
    std::vector<int>* v = new std::vector<int>(size);
    g_counting = false;
    CHECK(v->size() == 32);
    delete v;
    CHECK(g_allocations.load() == 2); // the vector object and its buffer
}

TEST_CASE("allocations: steady-state frame simulation does not touch the heap") {
    World world(2025, 3);
    Player player;
    player.setPosition(world.findSpawnPoint(0, 0));
    REQUIRE(world.waitForArea(player.eyePosition(), 6, 6, 120.0));
    // Drain everything that is ready, exactly like the game does each frame.
    MeshUpdate update;
    while (world.popMeshUpdate(update)) {
        world.recycleMesh(std::move(update.mesh));
    }
    ChunkPos unloaded;
    while (world.popUnloadedChunk(unloaded)) {
    }
    DayNightCycle cycle;
    MoveInput input;
    input.forward = 0.0f;
    world.update(player.eyePosition(), 6); // settle any pending scan

    g_allocations = 0;
    g_counting = true;
    for (int frame = 0; frame < 240; ++frame) {
        world.update(player.eyePosition(), 6);
        while (world.popMeshUpdate(update)) {
            world.recycleMesh(std::move(update.mesh));
        }
        while (world.popUnloadedChunk(unloaded)) {
        }
        player.addLook(0.01f, 0.0f);
        player.step(world, input, 1.0f / 120.0f);
        const auto hit = raycastBlocks(world, player.eyePosition(), player.forward(), 6.0f);
        (void)hit;
        cycle.advance(1.0f / 60.0f);
        const SkyState sky = cycle.sky();
        (void)sky;
        const Frustum frustum(Mat4::perspective(1.2f, 1.7f, 0.05f, 300.0f));
        (void)frustum.intersectsBox({0, 0, -20}, {16, 256, -4});
    }
    g_counting = false;
    CHECK(g_allocations.load() == 0);
}
