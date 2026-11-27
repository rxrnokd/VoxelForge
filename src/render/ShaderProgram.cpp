#include "render/ShaderProgram.hpp"

#include "rlgl.h"

#include <utility>

namespace voxelforge {

ShaderProgram::~ShaderProgram() { release(); }

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : m_shader(std::exchange(other.m_shader, Shader{})), m_valid(std::exchange(other.m_valid, false)) {}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
        release();
        m_shader = std::exchange(other.m_shader, Shader{});
        m_valid = std::exchange(other.m_valid, false);
    }
    return *this;
}

void ShaderProgram::release() {
    if (m_valid) {
        UnloadShader(m_shader);
    }
    m_shader = Shader{};
    m_valid = false;
}

bool ShaderProgram::load(const char* vertexSource, const char* fragmentSource) {
    release();
    Shader shader = LoadShaderFromMemory(vertexSource, fragmentSource);
    // raylib falls back to its default shader when compilation or linking fails.
    if (shader.id == 0 || shader.id == rlGetShaderIdDefault()) {
        return false;
    }
    m_shader = shader;
    m_valid = true;
    return true;
}

int ShaderProgram::location(const char* name) const {
    return m_valid ? GetShaderLocation(m_shader, name) : -1;
}

void ShaderProgram::bind() const { rlEnableShader(m_shader.id); }

void ShaderProgram::setInt(int loc, int value) { rlSetUniform(loc, &value, SHADER_UNIFORM_INT, 1); }

void ShaderProgram::setFloat(int loc, float value) { rlSetUniform(loc, &value, SHADER_UNIFORM_FLOAT, 1); }

void ShaderProgram::setVec2(int loc, float x, float y) {
    const float v[2] = {x, y};
    rlSetUniform(loc, v, SHADER_UNIFORM_VEC2, 1);
}

void ShaderProgram::setVec3(int loc, const Vec3& value) {
    const float v[3] = {value.x, value.y, value.z};
    rlSetUniform(loc, v, SHADER_UNIFORM_VEC3, 1);
}

void ShaderProgram::setVec4(int loc, float x, float y, float z, float w) {
    const float v[4] = {x, y, z, w};
    rlSetUniform(loc, v, SHADER_UNIFORM_VEC4, 1);
}

void ShaderProgram::setMat4(int loc, const Mat4& value) {
    // raylib's Matrix declares its fields row by row (m0, m4, m8, ...) but stores the
    // matrix column-major, so assign by name rather than copying memory.
    const auto& c = value.m;
    const Matrix m{c[0], c[4], c[8],  c[12], c[1], c[5], c[9],  c[13],
                   c[2], c[6], c[10], c[14], c[3], c[7], c[11], c[15]};
    rlSetUniformMatrix(loc, m);
}

GpuTexture::~GpuTexture() {
    if (m_texture.id != 0) {
        UnloadTexture(m_texture);
    }
}

GpuTexture::GpuTexture(GpuTexture&& other) noexcept : m_texture(std::exchange(other.m_texture, Texture2D{})) {}

GpuTexture& GpuTexture::operator=(GpuTexture&& other) noexcept {
    if (this != &other) {
        if (m_texture.id != 0) {
            UnloadTexture(m_texture);
        }
        m_texture = std::exchange(other.m_texture, Texture2D{});
    }
    return *this;
}

} // namespace voxelforge
