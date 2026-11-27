#pragma once

#include "core/Math.hpp"

#include "raylib.h"

namespace voxelforge {

// Move-only RAII owner of a raylib shader program.
class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    // Compiles and links; returns false (and logs) when the GPU rejects the source.
    bool load(const char* vertexSource, const char* fragmentSource);

    bool valid() const { return m_valid; }
    unsigned int id() const { return m_shader.id; }
    int location(const char* name) const;

    // Uniform setters operate on the currently bound program (call bind() first).
    void bind() const;
    static void setInt(int loc, int value);
    static void setFloat(int loc, float value);
    static void setVec2(int loc, float x, float y);
    static void setVec3(int loc, const Vec3& value);
    static void setVec4(int loc, float x, float y, float z, float w);
    static void setMat4(int loc, const Mat4& value);

private:
    void release();

    Shader m_shader{};
    bool m_valid = false;
};

// Move-only RAII owner of a raylib texture.
class GpuTexture {
public:
    GpuTexture() = default;
    explicit GpuTexture(Texture2D texture) : m_texture(texture) {}
    ~GpuTexture();
    GpuTexture(GpuTexture&& other) noexcept;
    GpuTexture& operator=(GpuTexture&& other) noexcept;
    GpuTexture(const GpuTexture&) = delete;
    GpuTexture& operator=(const GpuTexture&) = delete;

    const Texture2D& get() const { return m_texture; }
    bool valid() const { return m_texture.id != 0; }

private:
    Texture2D m_texture{};
};

} // namespace voxelforge
