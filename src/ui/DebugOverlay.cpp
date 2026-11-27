#include "ui/DebugOverlay.hpp"

#include "raylib.h"

#include <cmath>
#include <cstdio>

namespace voxelforge {
namespace {

const char* facingName(float yaw) {
    // Yaw 0 looks towards -Z (north); increasing yaw turns east.
    static constexpr const char* kNames[4] = {"North (-Z)", "East (+X)", "South (+Z)", "West (-X)"};
    float degrees = std::fmod(yaw * 180.0f / kPi, 360.0f);
    if (degrees < 0.0f) {
        degrees += 360.0f;
    }
    return kNames[static_cast<int>((degrees + 45.0f) / 90.0f) % 4];
}

void formatCount(char* out, std::size_t size, std::int64_t value) {
    if (value >= 1000000) {
        std::snprintf(out, size, "%.2fM", static_cast<double>(value) / 1e6);
    } else if (value >= 1000) {
        std::snprintf(out, size, "%.1fK", static_cast<double>(value) / 1e3);
    } else {
        std::snprintf(out, size, "%lld", static_cast<long long>(value));
    }
}

} // namespace

void DebugOverlay::draw(const DebugInfo& info) const {
    if (!m_visible) {
        return;
    }
    constexpr int kFont = 18;
    constexpr int kLineHeight = 21;
    constexpr int kLines = 13;
    char lines[kLines][128];
    int n = 0;

    const int bx = fastFloor(info.position.x);
    const int by = fastFloor(info.position.y);
    const int bz = fastFloor(info.position.z);
    const ChunkPos chunk = ChunkPos::fromWorld(bx, bz);
    char total[32];
    char visible[32];
    formatCount(total, sizeof(total), info.render.totalVertices);
    formatCount(visible, sizeof(visible), info.render.visibleVertices);

    std::snprintf(lines[n++], 128, "VoxelForge  (F3 to hide)");
    std::snprintf(lines[n++], 128, "FPS: %d  (%.2f ms)", info.fps, static_cast<double>(info.frameMs));
    std::snprintf(lines[n++], 128, "XYZ: %.2f / %.2f / %.2f", static_cast<double>(info.position.x),
                  static_cast<double>(info.position.y), static_cast<double>(info.position.z));
    std::snprintf(lines[n++], 128, "Block: %d %d %d   Chunk: %d, %d  (local %d, %d)", bx, by, bz, chunk.x, chunk.z,
                  bx - chunk.originX(), bz - chunk.originZ());
    std::snprintf(lines[n++], 128, "Facing: %s  yaw %.1f  pitch %.1f", facingName(info.yaw),
                  static_cast<double>(info.yaw * 180.0f / kPi), static_cast<double>(info.pitch * 180.0f / kPi));
    std::snprintf(lines[n++], 128, "Biome: %s   Seed: %llu", biomeName(info.biome),
                  static_cast<unsigned long long>(info.seed));
    std::snprintf(lines[n++], 128, "Time: %02d:%02d   %s%s", info.hours, info.minutes,
                  info.flying ? "flying" : "walking", info.onGround ? ", on ground" : "");
    std::snprintf(lines[n++], 128, "Chunks: %zu loaded, %zu meshed, %d visible  (render distance %d)",
                  info.world.loadedChunks, info.world.meshedChunks, info.render.visibleChunks, info.renderDistance);
    std::snprintf(lines[n++], 128, "Vertices: %s total, %s visible", total, visible);
    std::snprintf(lines[n++], 128, "Jobs: %zu generating, %zu meshing, %zu queued  (%u workers)",
                  info.world.generatingChunks, info.world.meshingChunks, info.world.queuedJobs, info.workerThreads);
    std::snprintf(lines[n++], 128, "Modified chunks: %zu", info.world.modifiedChunks);
    if (info.targetBlock) {
        std::snprintf(lines[n++], 128, "Target: %s at %d %d %d", blockName(info.targetType), info.targetBlock->x,
                      info.targetBlock->y, info.targetBlock->z);
    } else {
        std::snprintf(lines[n++], 128, "Target: none");
    }

    int width = 0;
    for (int i = 0; i < n; ++i) {
        const int w = MeasureText(lines[i], kFont);
        width = w > width ? w : width;
    }
    DrawRectangle(6, 6, width + 16, n * kLineHeight + 10, Color{0, 0, 0, 140});
    for (int i = 0; i < n; ++i) {
        DrawText(lines[i], 14, 12 + i * kLineHeight, kFont, i == 0 ? Color{255, 230, 120, 255} : RAYWHITE);
    }
}

} // namespace voxelforge
