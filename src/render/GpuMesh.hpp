#pragma once

#include "render/ChunkMesh.hpp"

#include <span>

namespace voxelforge {

// Move-only RAII owner of a VAO/VBO pair holding ChunkVertex triangles. Must only be
// used on the thread that owns the OpenGL context (the main thread).
class GpuMesh {
public:
    GpuMesh() = default;
    ~GpuMesh();
    GpuMesh(GpuMesh&& other) noexcept;
    GpuMesh& operator=(GpuMesh&& other) noexcept;
    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;

    // Uploads vertices, reusing the existing buffer when it is large enough.
    void upload(std::span<const ChunkVertex> vertices);
    void draw() const;
    void release();

    int vertexCount() const { return m_count; }
    bool empty() const { return m_count == 0; }

private:
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    int m_count = 0;
    int m_capacity = 0;
};

// Vertex-less VAO used for full-screen passes driven by gl_VertexID (core profile
// requires a bound VAO for every draw call).
class EmptyVertexArray {
public:
    EmptyVertexArray() = default;
    ~EmptyVertexArray();
    EmptyVertexArray(const EmptyVertexArray&) = delete;
    EmptyVertexArray& operator=(const EmptyVertexArray&) = delete;

    void create();
    void draw(int vertexCount) const;

private:
    unsigned int m_vao = 0;
};

} // namespace voxelforge
