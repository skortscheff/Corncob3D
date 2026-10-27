// Window.cpp - SDL2 window + renderer implementation
// Ported from 3dvid.inc video mode setup and 3.asm main loop

#include "Window.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace corncob {

Window::Window(const char* title, int width, int height)
    : m_title(title), m_width(width), m_height(height) {}

Window::~Window() {
    for (SDL_GameController* c : m_pads) SDL_GameControllerClose(c);
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window) SDL_DestroyWindow(m_window);
    SDL_Quit();
}

bool Window::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    m_window = SDL_CreateWindow(m_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                m_width, m_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!m_window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // vsync stands in for the original's page flip on vertical retrace
    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer) m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
    if (!m_renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        return false;
    }
    openPad();
    return true;
}

void Window::openPad() {
    for (SDL_GameController* c : m_pads) SDL_GameControllerClose(c);   // (opened twice is the same device, counted)
    m_pads.clear();
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i))
            if (SDL_GameController* c = SDL_GameControllerOpen(i)) m_pads.push_back(c);
}

void Window::padKey(SDL_Keycode sym, SDL_Scancode sc) {
    m_keyPresses.push_back({sym, false});
    if (m_lastKey == SDL_SCANCODE_UNKNOWN) m_lastKey = sc;
}

// the D-pad and the left stick as the arrow keys, pressed again every so often while held
void Window::padDirections() {
    int dir = 0;
    for (SDL_GameController* pad : m_pads) {
        auto btn = [&](SDL_GameControllerButton b) { return SDL_GameControllerGetButton(pad, b) != 0; };
        const float x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
        const float y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
        if (btn(SDL_CONTROLLER_BUTTON_DPAD_UP)) dir = SDLK_UP;
        else if (btn(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) dir = SDLK_DOWN;
        else if (btn(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) dir = SDLK_LEFT;
        else if (btn(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) dir = SDLK_RIGHT;
        else if (std::max(std::fabs(x), std::fabs(y)) > 0.6f)
            dir = std::fabs(y) >= std::fabs(x) ? (y < 0 ? SDLK_UP : SDLK_DOWN) : (x < 0 ? SDLK_LEFT : SDLK_RIGHT);
        if (dir) break;
    }
    const Uint32 now = SDL_GetTicks();
    if (dir == 0) {
        m_padDir = 0;
        return;
    }
    const SDL_Scancode sc = dir == SDLK_UP ? SDL_SCANCODE_UP : dir == SDLK_DOWN ? SDL_SCANCODE_DOWN : dir == SDLK_LEFT ? SDL_SCANCODE_LEFT : SDL_SCANCODE_RIGHT;
    if (dir != m_padDir) {   // a new direction: press, then wait a little before repeating
        m_padDir = dir;
        m_padRepeatAt = now + 400;
        padKey(dir, sc);
    } else if (now >= m_padRepeatAt) {
        m_padRepeatAt = now + 90;
        padKey(dir, sc);
    }
}

bool Window::pollEvents() {
    m_lastKey = SDL_SCANCODE_UNKNOWN;
    m_keyEvents.clear();
    m_keyPresses.clear();
    m_typed.clear();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) m_quit = true;
        if (e.type == SDL_KEYDOWN && !e.key.repeat && m_lastKey == SDL_SCANCODE_UNKNOWN)
            m_lastKey = e.key.keysym.scancode;
        if (e.type == SDL_KEYDOWN) m_keyPresses.push_back({e.key.keysym.sym, (e.key.keysym.mod & KMOD_SHIFT) != 0});
        if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED) openPad();
        if (e.type == SDL_CONTROLLERBUTTONDOWN) {   // the buttons that are not directions
            switch (e.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_A:
            case SDL_CONTROLLER_BUTTON_START: padKey(SDLK_RETURN, SDL_SCANCODE_RETURN); break;
            case SDL_CONTROLLER_BUTTON_B:
            case SDL_CONTROLLER_BUTTON_BACK: padKey(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE); break;
            case SDL_CONTROLLER_BUTTON_X: padKey(SDLK_SPACE, SDL_SCANCODE_SPACE); break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: padKey(SDLK_PAGEUP, SDL_SCANCODE_PAGEUP); break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: padKey(SDLK_PAGEDOWN, SDL_SCANCODE_PAGEDOWN); break;
            default: break;
            }
        }
        if (e.type == SDL_TEXTINPUT) m_typed += e.text.text;
        if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && !e.key.repeat)
            m_keyEvents.push_back({e.key.keysym.scancode, e.type == SDL_KEYDOWN, e.key.timestamp});
    }
    padDirections();
    return !m_quit;
}

} // namespace corncob
