// Window.cpp - SDL2 window + renderer implementation
// Ported from 3dvid.inc video mode setup and 3.asm main loop

#include "Window.hpp"
#include <iostream>

namespace corncob {

Window::Window(const char* title, int width, int height)
    : m_title(title), m_width(width), m_height(height) {}

Window::~Window() {
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window) SDL_DestroyWindow(m_window);
    SDL_Quit();
}

bool Window::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) < 0) {
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
    return true;
}

bool Window::pollEvents() {
    m_lastKey = SDL_SCANCODE_UNKNOWN;
    m_keyEvents.clear();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) m_quit = true;
        if (e.type == SDL_KEYDOWN && !e.key.repeat && m_lastKey == SDL_SCANCODE_UNKNOWN)
            m_lastKey = e.key.keysym.scancode;
        if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && !e.key.repeat)
            m_keyEvents.push_back({e.key.keysym.scancode, e.type == SDL_KEYDOWN, e.key.timestamp});
    }
    return !m_quit;
}

} // namespace corncob
