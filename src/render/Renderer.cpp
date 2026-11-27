#include "render/Renderer.hpp"

#include "EmbeddedShaders.hpp"
#include "render/Frustum.hpp"
#include "render/TextureAtlas.hpp"

#include "rlgl.h"

#include <algorithm>
#include <cstdio>

namespace voxelforge {

namespace {
constexpr float kWaterAlpha = 0.62f;
} // namespace

Renderer::ChunkUniforms Renderer::queryUniforms(const ShaderProgram& shader) {
    ChunkUniforms u;
    u.viewProj = shader.location("uViewProj");
    u.chunkOffset = shader.location("uChunkOffset");
    u.atlas = shader.location("uAtlas");
    u.sunDir = shader.location("uSunDir");
    u.sunColor = shader.location("uSunColor");
    u.ambient = shader.location("uAmbient");
    u.fogColor = shader.location("uFogColor");
    u.fogRange = shader.location("uFogRange");
    u.time = shader.location("uTime");
    u.alpha = shader.location("uAlpha");
    u.cameraPos = shader.location("uCameraPos");
    return u;
}

bool Renderer::init() {
    if (!m_chunkShader.load(shaders::kChunkVs, shaders::kChunkFs)) {
        std::fprintf(stderr, "voxelforge: failed to compile the chunk shader\n");
        return false;
    }
    if (!m_waterShader.load(shaders::kChunkVs, shaders::kWaterFs)) {
        std::fprintf(stderr, "voxelforge: failed to compile the water shader\n");
        return false;
    }
    m_chunkUniforms = queryUniforms(m_chunkShader);
    m_waterUniforms = queryUniforms(m_waterShader);

    const TextureAtlas atlas = TextureAtlas::generate();
    Image image{};
    image.data = const_cast<std::uint8_t*>(atlas.data().data()); // raylib copies the pixels to the GPU
    image.width = kAtlasWidth;
    image.height = kAtlasHeight;
    image.mipmaps = kAtlasMipLevels;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    m_atlas = GpuTexture(LoadTextureFromImage(image));
    if (!m_atlas.valid()) {
        std::fprintf(stderr, "voxelforge: failed to upload the texture atlas\n");
        return false;
    }
    SetTextureFilter(m_atlas.get(), TEXTURE_FILTER_POINT);
    SetTextureWrap(m_atlas.get(), TEXTURE_WRAP_CLAMP);
    return true;
}

void Renderer::setChunkMesh(ChunkPos pos, const ChunkMesh& mesh) {
    if (mesh.empty()) {
        removeChunk(pos);
        return;
    }
    ChunkGpuData& data = m_chunks[pos];
    data.opaque.upload(mesh.opaque);
    data.water.upload(mesh.water);
    data.minY = mesh.minY;
    data.maxY = mesh.maxY;
}

void Renderer::removeChunk(ChunkPos pos) { m_chunks.erase(pos); }

void Renderer::clearChunks() { m_chunks.clear(); }

void Renderer::collectVisible(const RenderCamera& camera) {
    m_visible.clear();
    m_stats.meshedChunks = static_cast<int>(m_chunks.size());
    m_stats.totalVertices = 0;
    m_stats.visibleVertices = 0;
    const Frustum frustum(camera.viewProjection());
    for (const auto& [pos, data] : m_chunks) {
        const std::int64_t vertices = data.opaque.vertexCount() + data.water.vertexCount();
        m_stats.totalVertices += vertices;
        // Camera-relative bounding box of the chunk's actual geometry.
        const Vec3 offset{static_cast<float>(pos.originX()) - camera.position.x, -camera.position.y,
                          static_cast<float>(pos.originZ()) - camera.position.z};
        const Vec3 boxMin{offset.x, offset.y + static_cast<float>(data.minY), offset.z};
        const Vec3 boxMax{offset.x + kChunkSize, offset.y + static_cast<float>(data.maxY), offset.z + kChunkSize};
        if (!frustum.intersectsBox(boxMin, boxMax)) {
            continue;
        }
        const float cx = offset.x + kChunkSize * 0.5f;
        const float cz = offset.z + kChunkSize * 0.5f;
        m_visible.push_back({&data, offset, cx * cx + cz * cz});
        m_stats.visibleVertices += vertices;
    }
    m_stats.visibleChunks = static_cast<int>(m_visible.size());
}

void Renderer::bindChunkShader(const ShaderProgram& shader, const ChunkUniforms& u, const Mat4& viewProj,
                               const RenderCamera& camera, const SceneLighting& lighting) const {
    shader.bind();
    ShaderProgram::setMat4(u.viewProj, viewProj);
    ShaderProgram::setInt(u.atlas, 0);
    ShaderProgram::setVec3(u.sunDir, lighting.sunDirection);
    ShaderProgram::setVec3(u.sunColor, lighting.sunColor);
    ShaderProgram::setVec3(u.ambient, lighting.ambientColor);
    ShaderProgram::setVec3(u.fogColor, lighting.fogColor);
    ShaderProgram::setVec2(u.fogRange, lighting.fogStart, lighting.fogEnd);
    if (u.time >= 0) {
        ShaderProgram::setFloat(u.time, lighting.timeSeconds);
    }
    if (u.alpha >= 0) {
        ShaderProgram::setFloat(u.alpha, kWaterAlpha);
    }
    if (u.cameraPos >= 0) {
        ShaderProgram::setVec3(u.cameraPos, camera.position);
    }
}

void Renderer::drawWorld(const RenderCamera& camera, const SceneLighting& lighting) {
    rlDrawRenderBatchActive(); // flush anything raylib has batched so far

    collectVisible(camera);
    const Mat4 viewProj = camera.viewProjection();

    rlActiveTextureSlot(0);
    rlEnableTexture(m_atlas.get().id);
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();

    // Opaque pass, front to back so early depth testing rejects hidden fragments.
    std::sort(m_visible.begin(), m_visible.end(),
              [](const VisibleChunk& a, const VisibleChunk& b) { return a.distanceSq < b.distanceSq; });
    bindChunkShader(m_chunkShader, m_chunkUniforms, viewProj, camera, lighting);
    for (const VisibleChunk& chunk : m_visible) {
        if (!chunk.data->opaque.empty()) {
            ShaderProgram::setVec3(m_chunkUniforms.chunkOffset, chunk.offset);
            chunk.data->opaque.draw();
        }
    }

    // Translucent water pass, back to front, blended, without depth writes.
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    rlSetBlendMode(RL_BLEND_ALPHA);
    bindChunkShader(m_waterShader, m_waterUniforms, viewProj, camera, lighting);
    for (auto it = m_visible.rbegin(); it != m_visible.rend(); ++it) {
        if (!it->data->water.empty()) {
            ShaderProgram::setVec3(m_waterUniforms.chunkOffset, it->offset);
            it->data->water.draw();
        }
    }

    // Restore the state raylib's 2D batch expects.
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
    rlDisableVertexArray();
    rlDisableShader();
    rlDisableTexture();
    rlDisableDepthTest();
}

} // namespace voxelforge

namespace voxelforge {

void Renderer::drawBlockOutline(const RenderCamera& camera, const IVec3& block) const {
    // Same steps as raylib's BeginMode3D/EndMode3D, but with our camera-relative matrices
    // so the lines share the depth buffer conventions of the chunk pass.
    rlDrawRenderBatchActive();
    const Mat4 projection = camera.projection();
    const Mat4 view = camera.viewRotation();
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();
    rlMultMatrixf(projection.m.data());
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    rlMultMatrixf(view.m.data());
    rlEnableDepthTest();

    const Vector3 center{static_cast<float>(block.x) + 0.5f - camera.position.x,
                         static_cast<float>(block.y) + 0.5f - camera.position.y,
                         static_cast<float>(block.z) + 0.5f - camera.position.z};
    DrawCubeWires(center, 1.006f, 1.006f, 1.006f, Color{15, 15, 15, 230});

    rlDrawRenderBatchActive();
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    rlDisableDepthTest();
}

} // namespace voxelforge
