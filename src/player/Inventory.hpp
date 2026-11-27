#pragma once

#include "world/Block.hpp"

#include <array>

namespace voxelforge {

// Nine-slot creative hotbar.
class Inventory {
public:
    static constexpr int kSlots = 9;

    Inventory();

    int selectedSlot() const { return m_selected; }
    BlockType selectedBlock() const { return m_slots[static_cast<std::size_t>(m_selected)]; }
    BlockType slot(int index) const { return m_slots[static_cast<std::size_t>(index)]; }

    void select(int index);
    // Mouse wheel: positive moves left, negative moves right, wrapping around.
    void scroll(float wheel);
    void setSlot(int index, BlockType type);
    // Middle-click "pick block": select the block if present, otherwise put it in the
    // current slot.
    void pick(BlockType type);

    const std::array<BlockType, kSlots>& slots() const { return m_slots; }

private:
    std::array<BlockType, kSlots> m_slots;
    int m_selected = 0;
};

} // namespace voxelforge
