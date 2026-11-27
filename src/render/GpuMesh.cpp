#include "render/GpuMesh.hpp"

#include "rlgl.h"

#include <utility>

namespace voxelforge {
namespace {
constexpr int kGlUnsignedShort = 0x1403; // GL_UNSIGNED_SHORT (rlgl has no named constant)

void describeLayout() {
    // One attribute: four unsigned shorts (x, y, z, packed data), converted to float.
    rlSetVertexAttribute(0, 4, kGlUnsignedShort, false, static_cast<int>(sizeof(ChunkVertex)), 0);
    rlEnableVertexAttribute(0);
}
} // namespace

GpuMesh::~GpuMesh() { release(); }

GpuMesh::GpuMesh(GpuMesh&& other) noexcept
    : m_vao(std::exchange(other.m_vao, 0u)),
      m_vbo(std::exchange(other.m_vbo, 0u)),
      m_count(std::exchange(other.m_count, 0)),
      m_capacity(std::exchange(other.m_capacity, 0)) {}

GpuMesh& GpuMesh::operator=(GpuMesh&& other) noexcept {
    if (this != &other) {
        release();
        m_vao = std::exchange(other.m_vao, 0u);
        m_vbo = std::exchange(other.m_vbo, 0u);
        m_count = std::exchange(other.m_count, 0);
        m_capacity = std::exchange(other.m_capacity, 0);
    }
    return *this;
}

void GpuMesh::release() {
    if (m_vbo != 0) {
        rlUnloadVertexBuffer(m_vbo);
    }
    if (m_vao != 0) {
        rlUnloadVertexArray(m_vao);
    }
    m_vao = 0;
    m_vbo = 0;
    m_count = 0;
    m_capacity = 0;
}

void GpuMesh::upload(std::span<const ChunkVertex> vertices) {
    const int count = static_cast<int>(vertices.size());
    if (count == 0) {
        release();
        return;
    }
    const int bytes = count * static_cast<int>(sizeof(ChunkVertex));
    if (m_vao != 0 && count <= m_capacity) {
        rlUpdateVertexBuffer(m_vbo, vertices.data(), bytes, 0);
        m_count = count;
        return;
    }
    if (m_vao == 0) {
        m_vao = rlLoadVertexArray();
    }
    rlEnableVertexArray(m_vao);
    if (m_vbo != 0) {
        rlUnloadVertexBuffer(m_vbo);
    }
    m_vbo = rlLoadVertexBuffer(vertices.data(), bytes, false);
    describeLayout();
    rlDisableVertexArray();
    m_count = count;
    m_capacity = count;
}

void GpuMesh::draw() const {
    if (m_count == 0) {
        return;
    }
    rlEnableVertexArray(m_vao);
    rlDrawVertexArray(0, m_count);
}

EmptyVertexArray::~EmptyVertexArray() {
    if (m_vao != 0) {
        rlUnloadVertexArray(m_vao);
    }
}

void EmptyVertexArray::create() {
    if (m_vao == 0) {
        m_vao = rlLoadVertexArray();
    }
}

void EmptyVertexArray::draw(int vertexCount) const {
    rlEnableVertexArray(m_vao);
    rlDrawVertexArray(0, vertexCount);
    rlDisableVertexArray();
}

} // namespace voxelforge
