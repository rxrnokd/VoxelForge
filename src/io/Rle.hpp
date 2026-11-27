#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace voxelforge {

// Run-length encoding for chunk data. The stream is a sequence of
//   [run length: unsigned LEB128 varint, >= 1] [value: 1 byte]
// pairs. Chunks are dominated by long runs (air above the terrain, stone below), so a
// 64 KiB chunk typically shrinks to a few KiB.

// Appends the encoding of `data` to `out`.
void rleEncode(std::span<const std::uint8_t> data, std::vector<std::uint8_t>& out);
std::vector<std::uint8_t> rleEncode(std::span<const std::uint8_t> data);

// Decodes into `out`, which must be exactly the original size. Returns false for any
// malformed input (truncated varint, zero length run, too much or too little data), so a
// corrupt save file can never write outside the destination buffer.
bool rleDecode(std::span<const std::uint8_t> encoded, std::span<std::uint8_t> out);

} // namespace voxelforge
