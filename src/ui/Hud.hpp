#pragma once

#include "player/Inventory.hpp"

#include "raylib.h"

namespace voxelforge {

// Crosshair, hotbar and the name of the selected block.
class Hud {
public:
    void notifySelectionChanged() { m_selectionAge = 0.0f; }
    void update(float dt) { m_selectionAge += dt; }

    void draw(const Texture2D& atlas, const Inventory& inventory, int screenWidth, int screenHeight) const;

    // Isometric cube icon built from the block's atlas tiles.
    static void drawBlockIcon(const Texture2D& atlas, BlockType type, float centerX, float centerY, float size);

private:
    float m_selectionAge = 10.0f;
};

} // namespace voxelforge
