#pragma once

#include "core/Math.hpp"
#include "render/ChunkMesh.hpp"
#include "render/GpuMesh.hpp"
#include "render/ShaderProgram.hpp"
#include "world/Chunk.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace voxelforge {

struct RenderCamera {
    Vec3 position;            // eye position in world space
    Vec3 forward{0, 0, -1};   // unit view direction
    float fovYRadians = 1.22f;
    float aspect = 16.0f / 9.0f;
    float nearPlane = 0.05f;
    float farPlane = 512.0f;
    float maxChunkDistance = 1e9f; // horizontal distance beyond which chunks are skipped

    // The world is rendered relative to the camera (the camera sits at the origin) so
    // float precision stays high far away from the world origin.
    Mat4 viewRotation() const { return Mat4::lookAt({0, 0, 0}, forward, {0, 1, 0}); }
    Mat4 projection() const { return Mat4::perspective(fovYRadians, aspect, nearPlane, farPlane); }
    Mat4 viewProjection() const { return projection() * viewRotation(); }
};

struct SceneLighting {
    Vec3 sunDirection{0.3f, 0.9f, 0.2f}; // towards the light source
    Vec3 sunColor{0.6f, 0.58f, 0.52f};
    Vec3 ambientColor{0.6f, 0.63f, 0.7f};
    Vec3 fogColor{0.62f, 0.76f, 0.95f};
    float fogStart = 100.0f;
    float fogEnd = 160.0f;
    float timeSeconds = 0.0f;
};

struct RenderStats {
    int meshedChunks = 0;
    int visibleChunks = 0;
    std::int64_t totalVertices = 0;
    std::int64_t visibleVertices = 0;
};

// Owns the GPU copies of chunk meshes and draws the voxel world.
class Renderer {
public:
    Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init(); // requires an OpenGL context

    void setChunkMesh(ChunkPos pos, const ChunkMesh& mesh);
    void removeChunk(ChunkPos pos);
    void clearChunks();
    bool hasChunk(ChunkPos pos) const { return m_chunks.contains(pos); }

    void drawWorld(const RenderCamera& camera, const SceneLighting& lighting);
    // Wireframe box around the targeted block, depth tested against the world.
    void drawBlockOutline(const RenderCamera& camera, const IVec3& block) const;

    const RenderStats& stats() const { return m_stats; }
    const Texture2D& atlasTexture() const { return m_atlas.get(); }

private:
    struct ChunkGpuData {
        GpuMesh opaque;
        GpuMesh water;
        int minY = 0;
        int maxY = 0;
    };
    struct VisibleChunk {
        const ChunkGpuData* data;
        Vec3 offset; // chunk origin relative to the camera
        float distanceSq;
    };
    struct ChunkUniforms {
        int viewProj = -1;
        int chunkOffset = -1;
        int atlas = -1;
        int sunDir = -1;
        int sunColor = -1;
        int ambient = -1;
        int fogColor = -1;
        int fogRange = -1;
        int time = -1;
        int alpha = -1;
        int cameraPos = -1;
    };

    void collectVisible(const RenderCamera& camera);
    void bindChunkShader(const ShaderProgram& shader, const ChunkUniforms& u, const Mat4& viewProj,
                         const RenderCamera& camera, const SceneLighting& lighting) const;
    static ChunkUniforms queryUniforms(const ShaderProgram& shader);

    std::unordered_map<ChunkPos, ChunkGpuData, ChunkPosHash> m_chunks;
    std::vector<VisibleChunk> m_visible; // reused every frame
    ShaderProgram m_chunkShader;
    ShaderProgram m_waterShader;
    ChunkUniforms m_chunkUniforms;
    ChunkUniforms m_waterUniforms;
    GpuTexture m_atlas;
    RenderStats m_stats;
};

} // namespace voxelforge
