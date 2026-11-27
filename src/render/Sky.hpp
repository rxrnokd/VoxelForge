#pragma once

#include "core/DayNightCycle.hpp"
#include "render/GpuMesh.hpp"
#include "render/Renderer.hpp"
#include "render/ShaderProgram.hpp"

namespace voxelforge {

// Draws the sky dome (gradient, square sun and moon, stars) as a full-screen pass.
class Sky {
public:
    bool init(); // requires an OpenGL context
    void draw(const RenderCamera& camera, const SkyState& sky) const;

private:
    ShaderProgram m_shader;
    EmptyVertexArray m_triangle;
    int m_invViewProj = -1;
    int m_sunDir = -1;
    int m_zenith = -1;
    int m_horizon = -1;
    int m_daylight = -1;
    int m_stars = -1;
};

// Fills the renderer's lighting parameters from the sky state and view distance.
SceneLighting makeSceneLighting(const SkyState& sky, float viewDistance, float timeSeconds, bool underwater);

} // namespace voxelforge
