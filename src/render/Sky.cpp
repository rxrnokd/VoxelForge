#include "render/Sky.hpp"

#include "EmbeddedShaders.hpp"

#include "rlgl.h"

#include <cstdio>

namespace voxelforge {

bool Sky::init() {
    if (!m_shader.load(shaders::kSkyVs, shaders::kSkyFs)) {
        std::fprintf(stderr, "voxelforge: failed to compile the sky shader\n");
        return false;
    }
    m_triangle.create();
    m_invViewProj = m_shader.location("uInvViewProj");
    m_sunDir = m_shader.location("uSunDir");
    m_zenith = m_shader.location("uZenith");
    m_horizon = m_shader.location("uHorizon");
    m_daylight = m_shader.location("uDaylight");
    m_stars = m_shader.location("uStars");
    return true;
}

void Sky::draw(const RenderCamera& camera, const SkyState& sky) const {
    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    rlDisableDepthMask();
    m_shader.bind();
    ShaderProgram::setMat4(m_invViewProj, camera.viewProjection().inverse());
    ShaderProgram::setVec3(m_sunDir, sky.sunDirection);
    ShaderProgram::setVec3(m_zenith, sky.zenithColor);
    ShaderProgram::setVec3(m_horizon, sky.horizonColor);
    ShaderProgram::setFloat(m_daylight, sky.daylight);
    ShaderProgram::setFloat(m_stars, sky.starVisibility);
    m_triangle.draw(3);
    rlDisableShader();
    rlEnableDepthMask();
}

SceneLighting makeSceneLighting(const SkyState& sky, float viewDistance, float timeSeconds, bool underwater) {
    SceneLighting l;
    l.sunDirection = sky.lightDirection;
    l.sunColor = sky.directLight;
    l.ambientColor = sky.ambientLight;
    l.fogColor = sky.fogColor;
    l.fogEnd = viewDistance - 8.0f;
    l.fogStart = l.fogEnd * 0.6f;
    l.timeSeconds = timeSeconds;
    if (underwater) {
        l.fogColor = Vec3{0.08f, 0.22f, 0.42f} * (0.25f + 0.75f * sky.daylight);
        l.fogStart = 0.0f;
        l.fogEnd = 28.0f;
    }
    return l;
}

} // namespace voxelforge
