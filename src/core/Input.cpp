#include "core/Input.hpp"

#include "raylib.h"

namespace voxelforge {

FrameInput Input::poll() {
    FrameInput in;

    in.move.forward = (IsKeyDown(KEY_W) ? 1.0f : 0.0f) - (IsKeyDown(KEY_S) ? 1.0f : 0.0f);
    in.move.right = (IsKeyDown(KEY_D) ? 1.0f : 0.0f) - (IsKeyDown(KEY_A) ? 1.0f : 0.0f);
    in.move.jump = IsKeyDown(KEY_SPACE);
    in.move.descend = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    in.move.sprint = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    const Vector2 delta = GetMouseDelta();
    if (m_captured && m_ignoreMouseFrames == 0) {
        in.mouseDeltaX = delta.x;
        in.mouseDeltaY = delta.y;
    }
    if (m_ignoreMouseFrames > 0) {
        --m_ignoreMouseFrames;
    }
    const Vector2 mouse = GetMousePosition();
    in.mouseX = mouse.x;
    in.mouseY = mouse.y;
    in.wheel = GetMouseWheelMove();

    in.breakPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    in.breakHeld = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    in.placePressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    in.placeHeld = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    in.pickPressed = IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);
    in.uiClick = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

    for (int i = 0; i < 9; ++i) {
        if (IsKeyPressed(KEY_ONE + i)) {
            in.hotbarSlot = i;
        }
    }

    in.toggleDebug = IsKeyPressed(KEY_F3);
    in.togglePause = IsKeyPressed(KEY_ESCAPE);
    in.toggleFly = IsKeyPressed(KEY_F);
    in.fastTime = IsKeyDown(KEY_T);
    in.takeScreenshot = IsKeyPressed(KEY_F2);
    return in;
}

void Input::setCursorCaptured(bool captured) {
    if (captured == m_captured) {
        return;
    }
    m_captured = captured;
    if (captured) {
        DisableCursor();
        m_ignoreMouseFrames = 2;
    } else {
        EnableCursor();
    }
}

} // namespace voxelforge
