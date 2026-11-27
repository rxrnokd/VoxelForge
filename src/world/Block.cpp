#include "world/Block.hpp"

namespace voxelforge {

static_assert(detail::kBlockTable[0].name[0] == 'A', "block table must start with Air");
static_assert(!isSolid(BlockType::Air) && !isOpaque(BlockType::Air), "air must be empty");
static_assert(isTranslucent(BlockType::Water) && !isSolid(BlockType::Water), "water is a non-solid translucent block");

const char* blockName(BlockType type) {
    return isValidBlock(static_cast<std::uint8_t>(type)) ? blockInfo(type).name : "Unknown";
}

} // namespace voxelforge
