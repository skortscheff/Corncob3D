// Input.cpp - Keyboard + joystick input implementation
// Ported from JOY.ASM, 3.asm keytable/joystick polling

#include "Input.hpp"
#include <cmath>

namespace corncob {

Input::~Input() {
    if (m_joy) SDL_JoystickClose(m_joy);
}

bool Input::initJoystick() {
    if (SDL_NumJoysticks() > 0) {
        m_joy = SDL_JoystickOpen(0);
    }
    return m_joy != nullptr;
}

void Input::update() {
    m_prevKeys = m_keys;

    int numKeys = 0;
    const Uint8* state = SDL_GetKeyboardState(&numKeys);
    for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
        m_keys[i] = i < numKeys && state[i];
    }

    if (m_joy) {
        int axes = SDL_JoystickNumAxes(m_joy);
        for (int i = 0; i < 4; i++) {
            float v = i < axes ? SDL_JoystickGetAxis(m_joy, i) / 32767.0f : 0.0f;
            m_axis[i] = std::fabs(v) < 0.12f ? 0.0f : v; // deadzone, like guardb in 3.asm
        }
        int buttons = SDL_JoystickNumButtons(m_joy);
        for (int i = 0; i < 4; i++) {
            m_buttons[i] = i < buttons && SDL_JoystickGetButton(m_joy, i);
        }
    }
}

} // namespace corncob
