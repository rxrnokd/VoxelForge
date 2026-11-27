#include "ui/PauseMenu.hpp"

#include "core/Math.hpp"

#include "raylib.h"

#include <cmath>
#include <cstdio>

namespace voxelforge {
namespace {

bool inside(const FrameInput& in, float x, float y, float w, float h) {
    return in.mouseX >= x && in.mouseX <= x + w && in.mouseY >= y && in.mouseY <= y + h;
}

void centeredText(const char* text, float cx, float y, int size, Color color) {
    const int w = MeasureText(text, size);
    DrawText(text, static_cast<int>(cx) - w / 2, static_cast<int>(y), size, color);
}

} // namespace

bool PauseMenu::button(const char* label, float x, float y, float w, float h, const FrameInput& input,
                       bool enabled) const {
    const bool hover = enabled && inside(input, x, y, w, h);
    const Color fill = !enabled ? Color{60, 60, 60, 200} : (hover ? Color{110, 140, 200, 235} : Color{70, 80, 100, 220});
    DrawRectangleRounded(Rectangle{x, y, w, h}, 0.2f, 6, fill);
    DrawRectangleRoundedLinesEx(Rectangle{x, y, w, h}, 0.2f, 6, 2.0f, Color{20, 20, 30, 255});
    centeredText(label, x + w * 0.5f, y + h * 0.5f - 10.0f, 20, enabled ? RAYWHITE : GRAY);
    return hover && input.uiClick && m_activeSlider < 0;
}

bool PauseMenu::slider(const char* label, float& value, float minValue, float maxValue, float x, float y, float w,
                       const FrameInput& input, const char* format) {
    const int index = m_sliderIndex++;
    const float trackY = y + 30.0f;
    const float trackH = 10.0f;
    const bool mouseDown = input.breakHeld;

    if (m_activeSlider < 0 && input.breakPressed && inside(input, x - 8.0f, trackY - 10.0f, w + 16.0f, trackH + 20.0f)) {
        m_activeSlider = index;
    }
    bool changed = false;
    if (m_activeSlider == index) {
        if (mouseDown) {
            const float t = clampf((input.mouseX - x) / w, 0.0f, 1.0f);
            const float next = minValue + t * (maxValue - minValue);
            changed = next != value;
            value = next;
        } else {
            m_activeSlider = -1;
        }
    }

    char text[96];
    char number[32];
    std::snprintf(number, sizeof(number), format, static_cast<double>(value));
    std::snprintf(text, sizeof(text), "%s: %s", label, number);
    DrawText(text, static_cast<int>(x), static_cast<int>(y), 20, RAYWHITE);
    DrawRectangleRounded(Rectangle{x, trackY, w, trackH}, 0.5f, 4, Color{40, 40, 50, 230});
    const float t = (value - minValue) / (maxValue - minValue);
    DrawRectangleRounded(Rectangle{x, trackY, w * t, trackH}, 0.5f, 4, Color{110, 150, 220, 255});
    DrawCircle(static_cast<int>(x + w * t), static_cast<int>(trackY + trackH * 0.5f), 10.0f,
               m_activeSlider == index ? Color{255, 255, 255, 255} : Color{220, 225, 235, 255});
    return changed;
}

PauseMenu::Action PauseMenu::draw(Settings& settings, const FrameInput& input, int screenWidth, int screenHeight,
                                  const char* statusMessage, bool savingEnabled) {
    m_sliderIndex = 0;
    DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 150});

    const float panelW = 460.0f;
    const float panelH = 520.0f;
    const float px = (static_cast<float>(screenWidth) - panelW) * 0.5f;
    const float py = (static_cast<float>(screenHeight) - panelH) * 0.5f;
    DrawRectangleRounded(Rectangle{px, py, panelW, panelH}, 0.06f, 8, Color{25, 28, 38, 235});
    centeredText("Game Paused", px + panelW * 0.5f, py + 20.0f, 32, Color{255, 230, 120, 255});

    const float x = px + 40.0f;
    const float w = panelW - 80.0f;
    float y = py + 80.0f;

    slider("Mouse sensitivity", settings.mouseSensitivity, Settings::kMinSensitivity, 0.5f, x, y, w, input, "%.3f");
    y += 70.0f;
    float distance = static_cast<float>(settings.renderDistance);
    slider("Render distance", distance, static_cast<float>(Settings::kMinRenderDistance),
           static_cast<float>(Settings::kMaxRenderDistance), x, y, w, input, "%.0f chunks");
    settings.renderDistance = static_cast<int>(std::lround(distance));
    y += 70.0f;
    slider("Field of view", settings.fovDegrees, 50.0f, 110.0f, x, y, w, input, "%.0f deg");
    y += 80.0f;

    Action action = Action::None;
    const float buttonH = 44.0f;
    if (button("Resume", x, y, w, buttonH, input, true)) action = Action::Resume;
    y += buttonH + 12.0f;
    if (button("Save World", x, y, w, buttonH, input, savingEnabled)) action = Action::Save;
    y += buttonH + 12.0f;
    if (button(savingEnabled ? "Save & Quit" : "Quit", x, y, w, buttonH, input, true)) action = Action::SaveAndQuit;
    y += buttonH + 16.0f;

    if (statusMessage != nullptr && statusMessage[0] != '\0') {
        centeredText(statusMessage, px + panelW * 0.5f, y, 18, Color{160, 230, 160, 255});
    }
    settings.clampToLimits();
    return action;
}

} // namespace voxelforge
