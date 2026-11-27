#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

namespace voxelforge {

// Little-endian binary writer. Values are serialised byte by byte, so files are
// identical on every platform regardless of the host byte order.
class ByteWriter {
public:
    void u8(std::uint8_t v) { m_bytes.push_back(v); }
    void u16(std::uint16_t v) { putLE(v, 2); }
    void u32(std::uint32_t v) { putLE(v, 4); }
    void u64(std::uint64_t v) { putLE(v, 8); }
    void i32(std::int32_t v) { u32(static_cast<std::uint32_t>(v)); }
    void f32(float v) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &v, sizeof(bits));
        u32(bits);
    }
    void bytes(std::span<const std::uint8_t> data) { m_bytes.insert(m_bytes.end(), data.begin(), data.end()); }

    // Overwrites a previously written u32/u64 (used to back-patch sizes and checksums).
    void patchU32(std::size_t offset, std::uint32_t v) { patchLE(offset, v, 4); }
    void patchU64(std::size_t offset, std::uint64_t v) { patchLE(offset, v, 8); }

    std::size_t size() const { return m_bytes.size(); }
    const std::vector<std::uint8_t>& data() const { return m_bytes; }
    std::vector<std::uint8_t> take() { return std::move(m_bytes); }

private:
    void putLE(std::uint64_t v, int count) {
        for (int i = 0; i < count; ++i) {
            m_bytes.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }
    void patchLE(std::size_t offset, std::uint64_t v, int count) {
        for (int i = 0; i < count; ++i) {
            m_bytes[offset + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v >> (8 * i));
        }
    }

    std::vector<std::uint8_t> m_bytes;
};

// Bounds-checked little-endian reader. Every read returns false instead of reading past
// the end, which makes parsing untrusted files safe.
class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> data) : m_data(data) {}

    bool u8(std::uint8_t& v) { return getLE(v, 1); }
    bool u16(std::uint16_t& v) { return getLE(v, 2); }
    bool u32(std::uint32_t& v) { return getLE(v, 4); }
    bool u64(std::uint64_t& v) { return getLE(v, 8); }
    bool i32(std::int32_t& v) {
        std::uint32_t u = 0;
        if (!u32(u)) return false;
        v = static_cast<std::int32_t>(u);
        return true;
    }
    bool f32(float& v) {
        std::uint32_t u = 0;
        if (!u32(u)) return false;
        std::memcpy(&v, &u, sizeof(v));
        return true;
    }
    bool bytes(std::size_t count, std::span<const std::uint8_t>& out) {
        if (count > remaining()) return false;
        out = m_data.subspan(m_pos, count);
        m_pos += count;
        return true;
    }

    std::size_t remaining() const { return m_data.size() - m_pos; }
    std::size_t position() const { return m_pos; }

private:
    template <typename T>
    bool getLE(T& v, int count) {
        if (static_cast<std::size_t>(count) > remaining()) return false;
        std::uint64_t value = 0;
        for (int i = 0; i < count; ++i) {
            value |= static_cast<std::uint64_t>(m_data[m_pos + static_cast<std::size_t>(i)]) << (8 * i);
        }
        m_pos += static_cast<std::size_t>(count);
        v = static_cast<T>(value);
        return true;
    }

    std::span<const std::uint8_t> m_data;
    std::size_t m_pos = 0;
};

} // namespace voxelforge
