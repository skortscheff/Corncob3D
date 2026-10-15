// Window.hpp - SDL2 window + renderer wrapper
// Ported from 3dvid.inc: video mode constants, page flipping

#pragma once
#include <SDL.h>

namespace corncob {

class Window {
public:
    // Default window is the EGA 640x350 frame scaled 2x, stretched to 4:3 like a CRT
    Window(const char* title = "Corncob 3-D", int width = 1280, int height = 960);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool init();

    // Pump events; returns false once the user asked to quit (window close)
    bool pollEvents();

    // First key pressed during the last pollEvents (SDL_SCANCODE_UNKNOWN if none)
    SDL_Scancode lastKey() const { return m_lastKey; }

    bool shouldQuit() const { return m_quit; }
    void setQuit() { m_quit = true; }

    SDL_Window* sdlWindow() const { return m_window; }
    SDL_Renderer* sdlRenderer() const { return m_renderer; }

private:
    const char* m_title;
    int m_width, m_height;
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    bool m_quit = false;
    SDL_Scancode m_lastKey = SDL_SCANCODE_UNKNOWN;
};

} // namespace corncob
