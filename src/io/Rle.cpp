#include "io/Rle.hpp"

#include <algorithm>
#include <cstring>

namespace voxelforge {
namespace {

void writeVarint(std::uint32_t value, std::vector<std::uint8_t>& out) {
    while (value >= 0x80u) {
        out.push_back(static_cast<std::uint8_t>(value | 0x80u));
        value >>= 7;
    }
    out.push_back(static_cast<std::uint8_t>(value));
}

bool readVarint(std::span<const std::uint8_t> in, std::size_t& pos, std::uint32_t& value) {
    value = 0;
    for (int shift = 0; shift < 35; shift += 7) {
        if (pos >= in.size()) {
            return false;
        }
        const std::uint8_t byte = in[pos++];
        if (shift == 28 && (byte & 0xF0u) != 0) {
            return false; // would overflow 32 bits
        }
        value |= static_cast<std::uint32_t>(byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

void rleEncode(std::span<const std::uint8_t> data, std::vector<std::uint8_t>& out) {
    std::size_t i = 0;
    while (i < data.size()) {
        const std::uint8_t value = data[i];
        std::size_t run = 1;
        while (i + run < data.size() && data[i + run] == value && run < 0xFFFFFFFFu) {
            ++run;
        }
        writeVarint(static_cast<std::uint32_t>(run), out);
        out.push_back(value);
        i += run;
    }
}

std::vector<std::uint8_t> rleEncode(std::span<const std::uint8_t> data) {
    std::vector<std::uint8_t> out;
    rleEncode(data, out);
    return out;
}

bool rleDecode(std::span<const std::uint8_t> encoded, std::span<std::uint8_t> out) {
    std::size_t in = 0;
    std::size_t written = 0;
    while (in < encoded.size()) {
        std::uint32_t run = 0;
        if (!readVarint(encoded, in, run) || run == 0 || in >= encoded.size()) {
            return false;
        }
        const std::uint8_t value = encoded[in++];
        if (run > out.size() - written) {
            return false; // would overflow the destination
        }
        std::memset(out.data() + written, value, run);
        written += run;
    }
    return written == out.size();
}

} // namespace voxelforge
