#include "doctest.h"

#include "core/Hash.hpp"
#include "io/Rle.hpp"
#include "world/TerrainGenerator.hpp"

#include <algorithm>
#include <memory>
#include <vector>

using namespace voxelforge;

namespace {
bool roundTrips(const std::vector<std::uint8_t>& data) {
    const std::vector<std::uint8_t> encoded = rleEncode(data);
    std::vector<std::uint8_t> decoded(data.size(), 0xAB);
    return rleDecode(encoded, decoded) && decoded == data;
}
} // namespace

TEST_CASE("rle: round trips assorted inputs") {
    CHECK(roundTrips({}));
    CHECK(roundTrips({7}));
    CHECK(roundTrips(std::vector<std::uint8_t>(65536, 0)));
    CHECK(roundTrips(std::vector<std::uint8_t>(300, 9)));
    std::vector<std::uint8_t> alternating(1000);
    for (std::size_t i = 0; i < alternating.size(); ++i) alternating[i] = static_cast<std::uint8_t>(i & 1);
    CHECK(roundTrips(alternating));
    std::vector<std::uint8_t> random(5000);
    SplitMix64 rng(3);
    for (auto& b : random) b = static_cast<std::uint8_t>(rng.nextBelow(4));
    CHECK(roundTrips(random));
    // Runs longer than 127 need multi-byte varints.
    std::vector<std::uint8_t> mixed(20201, 2);
    std::fill_n(mixed.begin(), 200, std::uint8_t{1});
    mixed.back() = 3;
    CHECK(roundTrips(mixed));
}

TEST_CASE("rle: compresses real chunks well") {
    const TerrainGenerator gen(2024);
    auto chunk = std::make_unique<Chunk>();
    gen.generate({3, -2}, *chunk);
    const std::vector<std::uint8_t> encoded = rleEncode(chunk->bytes());
    CHECK(encoded.size() * 4 < static_cast<std::size_t>(kChunkVolume));
    auto back = std::make_unique<Chunk>();
    REQUIRE(rleDecode(encoded, back->bytes()));
    CHECK(back->contentHash() == chunk->contentHash());
}

TEST_CASE("rle: malformed input is rejected without overflowing") {
    std::vector<std::uint8_t> out(10);
    CHECK_FALSE(rleDecode(std::vector<std::uint8_t>{5, 1}, out));        // too short
    CHECK_FALSE(rleDecode(std::vector<std::uint8_t>{11, 1}, out));       // too long
    CHECK_FALSE(rleDecode(std::vector<std::uint8_t>{10}, out));          // missing value
    CHECK_FALSE(rleDecode(std::vector<std::uint8_t>{0, 1, 10, 1}, out)); // zero length run
    CHECK_FALSE(rleDecode(std::vector<std::uint8_t>{0x80}, out));        // truncated varint
    CHECK_FALSE(rleDecode(std::vector<std::uint8_t>{0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 1}, out)); // overflow
    CHECK(rleDecode(std::vector<std::uint8_t>{4, 1, 6, 2}, out));
    CHECK(out[3] == 1);
    CHECK(out[4] == 2);
}
