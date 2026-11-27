#pragma once

#include "player/Player.hpp"

namespace voxelforge {

// Everything the game needs from the keyboard and mouse for one frame. Gathering it in
// one place keeps raylib calls out of the gameplay code.
struct FrameInput {
    MoveInput move;
    float mouseDeltaX = 0.0f; // pixels
    float mouseDeltaY = 0.0f;
    float mouseX = 0.0f;      // cursor position (menus)
    float mouseY = 0.0f;
    float wheel = 0.0f;

    bool breakPressed = false; // left mouse
    bool breakHeld = false;
    bool placePressed = false; // right mouse
    bool placeHeld = false;
    bool pickPressed = false;  // middle mouse
    bool uiClick = false;      // left click released (menus)

    int hotbarSlot = -1; // 0..8 when a number key was pressed this frame

    bool toggleDebug = false; // F3
    bool togglePause = false; // Esc
    bool toggleFly = false;   // F
    bool fastTime = false;    // T held: speed up the day/night cycle
    bool takeScreenshot = false; // F2
};

class Input {
public:
    // Reads the current keyboard/mouse state. Must be called once per frame.
    FrameInput poll();

    // Captures (true) or releases (false) the mouse cursor.
    void setCursorCaptured(bool captured);
    bool cursorCaptured() const { return m_captured; }

private:
    bool m_captured = false;
    int m_ignoreMouseFrames = 0; // the first delta after capturing is a jump; drop it
};

} // namespace voxelforge
