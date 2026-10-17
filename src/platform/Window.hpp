// Window.hpp - SDL2 window + renderer wrapper
// Ported from 3dvid.inc: video mode constants, page flipping

#pragma once
#include <SDL.h>
#include <string>
#include <vector>

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

    // Every key press and release seen during the last pollEvents, in order, with the
    // SDL timestamp (ms) so the keyboard emulation can place them within the frame.
    // Auto-repeats are dropped.
    struct KeyEvent {
        SDL_Scancode scancode;
        bool down;
        Uint32 timestampMs;
    };
    const std::vector<KeyEvent>& keyEvents() const { return m_keyEvents; }

    // Menu input: every key press including auto-repeats (SDL keycode, shift held), and the text typed
    // (SDL_StartTextInput must be on for the text)
    struct KeyPress {
        SDL_Keycode code;
        bool shift;
    };
    const std::vector<KeyPress>& keyPresses() const { return m_keyPresses; }
    const std::string& typedText() const { return m_typed; }

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
    std::vector<KeyEvent> m_keyEvents;
    std::vector<KeyPress> m_keyPresses;
    std::string m_typed;
};

} // namespace corncob
