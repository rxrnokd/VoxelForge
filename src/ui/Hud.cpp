#include "ui/Hud.hpp"

#include "core/Math.hpp"
#include "render/TextureAtlas.hpp"

#include "rlgl.h"

#include <algorithm>

namespace voxelforge {
namespace {

struct Point {
    float x;
    float y;
};

// Draws a textured quad whose corners are given in screen space, in raylib's winding
// order (top-left, bottom-left, bottom-right, top-right of the texture).
void texturedQuad(Tile tile, const Point (&p)[4], unsigned char shade) {
    constexpr float kInset = 0.05f; // stay inside the tile with point sampling
    const float u0 = (static_cast<float>(TextureAtlas::tileX(tile)) + kInset) / kAtlasWidth;
    const float v0 = (static_cast<float>(TextureAtlas::tileY(tile)) + kInset) / kAtlasHeight;
    const float u1 = (static_cast<float>(TextureAtlas::tileX(tile) + kTilePixels) - kInset) / kAtlasWidth;
    const float v1 = (static_cast<float>(TextureAtlas::tileY(tile) + kTilePixels) - kInset) / kAtlasHeight;
    const float uv[4][2] = {{u0, v0}, {u0, v1}, {u1, v1}, {u1, v0}};
    rlColor4ub(shade, shade, shade, 255);
    for (int i = 0; i < 4; ++i) {
        rlTexCoord2f(uv[i][0], uv[i][1]);
        rlVertex2f(p[i].x, p[i].y);
    }
}

} // namespace

void Hud::drawBlockIcon(const Texture2D& atlas, BlockType type, float cx, float cy, float size) {
    const float h = size * 0.5f;
    const float w = h * 0.866f;
    const Point top{cx, cy - h};
    const Point left{cx - w, cy - h * 0.5f};
    const Point right{cx + w, cy - h * 0.5f};
    const Point center{cx, cy};
    const Point leftLow{cx - w, cy + h * 0.5f};
    const Point rightLow{cx + w, cy + h * 0.5f};
    const Point bottom{cx, cy + h};

    rlSetTexture(atlas.id);
    rlBegin(RL_QUADS);
    texturedQuad(blockTile(type, Face::PosY), {top, left, center, right}, 255);
    texturedQuad(blockTile(type, Face::NegX), {left, leftLow, bottom, center}, 205);
    texturedQuad(blockTile(type, Face::PosZ), {center, bottom, rightLow, right}, 160);
    rlEnd();
    rlSetTexture(0);
}

void Hud::draw(const Texture2D& atlas, const Inventory& inventory, int screenWidth, int screenHeight) const {
    // Crosshair
    const int cx = screenWidth / 2;
    const int cy = screenHeight / 2;
    DrawRectangle(cx - 10, cy - 1, 20, 2, Color{255, 255, 255, 210});
    DrawRectangle(cx - 1, cy - 10, 2, 20, Color{255, 255, 255, 210});

    // Hotbar
    const int slot = std::clamp(screenHeight / 14, 36, 72);
    const int gap = 4;
    const int total = Inventory::kSlots * slot + (Inventory::kSlots - 1) * gap;
    const int x0 = (screenWidth - total) / 2;
    const int y0 = screenHeight - slot - 12;
    DrawRectangle(x0 - 6, y0 - 6, total + 12, slot + 12, Color{0, 0, 0, 110});
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const int x = x0 + i * (slot + gap);
        const bool selected = i == inventory.selectedSlot();
        DrawRectangle(x, y0, slot, slot, selected ? Color{255, 255, 255, 70} : Color{30, 30, 30, 150});
        drawBlockIcon(atlas, inventory.slot(i), static_cast<float>(x) + slot * 0.5f, static_cast<float>(y0) + slot * 0.5f,
                      static_cast<float>(slot) * 0.72f);
        if (selected) {
            DrawRectangleLinesEx(Rectangle{static_cast<float>(x) - 2, static_cast<float>(y0) - 2,
                                           static_cast<float>(slot) + 4, static_cast<float>(slot) + 4},
                                 3.0f, RAYWHITE);
        }
        char label[4] = {static_cast<char>('1' + i), '\0', '\0', '\0'};
        DrawText(label, x + 3, y0 + 2, 10, Color{230, 230, 230, 200});
    }

    // Selected block name fades out two seconds after a change.
    if (m_selectionAge < 2.5f) {
        const float alpha = clampf(2.5f - m_selectionAge, 0.0f, 1.0f);
        const char* name = blockName(inventory.selectedBlock());
        const int fontSize = 20;
        const int width = MeasureText(name, fontSize);
        const auto a = static_cast<unsigned char>(alpha * 255.0f);
        DrawText(name, (screenWidth - width) / 2 + 1, y0 - 34 + 1, fontSize, Color{0, 0, 0, a});
        DrawText(name, (screenWidth - width) / 2, y0 - 34, fontSize, Color{255, 255, 255, a});
    }
}

} // namespace voxelforge
