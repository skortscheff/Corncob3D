// Input.cpp - Keyboard + joystick input implementation
// Ported from JOY.ASM, 3.asm keytable/joystick polling

#include "Input.hpp"
#include <algorithm>
#include <cmath>

namespace corncob {

Input::~Input() {
    if (m_pad) SDL_GameControllerClose(m_pad);
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

    // game controller: look for one now and then, drop it when it goes away
    if (m_pad && !SDL_GameControllerGetAttached(m_pad)) {
        SDL_GameControllerClose(m_pad);
        m_pad = nullptr;
    }
    if (!m_pad && (m_padCheck++ % 60) == 0) {
        for (int i = 0; i < SDL_NumJoysticks(); i++)
            if (SDL_IsGameController(i) && (m_pad = SDL_GameControllerOpen(i)) != nullptr) break;
    }
    if (m_pad) {
        auto stick = [&](SDL_GameControllerAxis ax, SDL_GameControllerAxis ay) {
            float x = SDL_GameControllerGetAxis(m_pad, ax) / 32767.0f, y = SDL_GameControllerGetAxis(m_pad, ay) / 32767.0f;
            const float mag = std::sqrt(x * x + y * y), dead = 0.15f;
            if (mag < dead) { x = y = 0.0f; }
            else {   // radial dead zone, rescaled so the edge of it is zero
                const float k = (std::min(mag, 1.0f) - dead) / (1.0f - dead) / mag;
                x = std::max(-1.0f, std::min(1.0f, x * k));
                y = std::max(-1.0f, std::min(1.0f, y * k));
            }
            m_padAxis[ax] = x;
            m_padAxis[ay] = y;
        };
        stick(SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY);
        stick(SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY);
        m_padAxis[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = SDL_GameControllerGetAxis(m_pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0f;
        m_padAxis[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] = SDL_GameControllerGetAxis(m_pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0f;
        for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++)
            m_padButtons[b] = SDL_GameControllerGetButton(m_pad, static_cast<SDL_GameControllerButton>(b)) != 0;
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
