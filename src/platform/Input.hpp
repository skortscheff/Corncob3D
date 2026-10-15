// Input.hpp - Keyboard + joystick input handling
// Ported from JOY.ASM (joystick polling), 3.asm kbdtbl/keytable

#pragma once
#include <SDL.h>
#include <array>

namespace corncob {

class Input {
public:
    Input() = default;
    ~Input();

    // Opens the first joystick if present (JOY.ASM detection)
    bool initJoystick();

    // Snapshot keyboard/joystick state; call once per frame after pumping events
    void update();

    bool isKeyDown(SDL_Scancode code) const { return m_keys[code]; }
    bool isKeyJustDown(SDL_Scancode code) const { return m_keys[code] && !m_prevKeys[code]; }
    bool isKeyJustUp(SDL_Scancode code) const { return !m_keys[code] && m_prevKeys[code]; }

    // Joystick axes normalized to [-1, 1] with a deadzone (replaces jleft/jrght calibration)
    float joystickAxis(int axis) const { return (axis >= 0 && axis < 4) ? m_axis[axis] : 0.0f; }
    bool joystickButton(int btn) const { return (btn >= 0 && btn < 4) ? m_buttons[btn] : false; }
    bool hasJoystick() const { return m_joy != nullptr; }

private:
    std::array<bool, SDL_NUM_SCANCODES> m_keys{};
    std::array<bool, SDL_NUM_SCANCODES> m_prevKeys{};

    SDL_Joystick* m_joy = nullptr;
    float m_axis[4] = {};
    bool m_buttons[4] = {};
};

} // namespace corncob
