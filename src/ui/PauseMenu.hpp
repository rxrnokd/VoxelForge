#pragma once

#include "core/Input.hpp"
#include "core/Settings.hpp"

namespace voxelforge {

// Esc menu: resume, mouse sensitivity, render distance, field of view, save and quit.
// A tiny immediate-mode UI: widgets are drawn and hit-tested in the same call.
class PauseMenu {
public:
    enum class Action { None, Resume, Save, SaveAndQuit };

    // Draws the menu, applies slider changes to `settings` and returns the clicked action.
    Action draw(Settings& settings, const FrameInput& input, int screenWidth, int screenHeight,
                const char* statusMessage, bool savingEnabled);

private:
    bool button(const char* label, float x, float y, float w, float h, const FrameInput& input, bool enabled) const;
    bool slider(const char* label, float& value, float minValue, float maxValue, float x, float y, float w,
                const FrameInput& input, const char* format);

    int m_activeSlider = -1; // slider being dragged
    int m_sliderIndex = 0;   // running index while drawing
};

} // namespace voxelforge
