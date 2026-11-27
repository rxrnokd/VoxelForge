#include "player/Inventory.hpp"

#include "core/Math.hpp"

namespace voxelforge {

Inventory::Inventory()
    : m_slots{BlockType::Grass, BlockType::Dirt,   BlockType::Stone, BlockType::Sand,  BlockType::Wood,
              BlockType::Leaves, BlockType::Glass, BlockType::Brick, BlockType::Water} {}

void Inventory::select(int index) {
    if (index >= 0 && index < kSlots) {
        m_selected = index;
    }
}

void Inventory::scroll(float wheel) {
    if (wheel > 0.0f) {
        m_selected = floorMod(m_selected - 1, kSlots);
    } else if (wheel < 0.0f) {
        m_selected = floorMod(m_selected + 1, kSlots);
    }
}

void Inventory::setSlot(int index, BlockType type) {
    if (index >= 0 && index < kSlots && type != BlockType::Air && isValidBlock(static_cast<std::uint8_t>(type))) {
        m_slots[static_cast<std::size_t>(index)] = type;
    }
}

void Inventory::pick(BlockType type) {
    if (type == BlockType::Air) {
        return;
    }
    for (int i = 0; i < kSlots; ++i) {
        if (m_slots[static_cast<std::size_t>(i)] == type) {
            m_selected = i;
            return;
        }
    }
    setSlot(m_selected, type);
}

} // namespace voxelforge
