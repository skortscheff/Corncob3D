// TextScreen.hpp - An 80x25 colour text screen (the look of the original's menus) and its keyboard
//
// The menu programs of the original (MOAG, TOWER, ISCORE) ran in the PC's text mode. This is that mode
// drawn on a 640x350 frame buffer with 8x14 cells: every cell is a character with a 16 colour
// foreground and a 16 colour background (attribute = bg << 4 | fg). The text of the original's menus
// marks what to highlight with backslashes: "\Duty roster" highlights the D (the hot key), and
// "\(some words\)" highlights everything between the brackets; both are understood by putMarkup.
//
// Keys come from the window, or from a script (--ui-keys) when the screens are checked unattended.

#pragma once
#include "platform/Window.hpp"
#include "render/FrameBuffer.hpp"
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace corncob {

// The colours of a text attribute
enum : uint8_t {
    BLACK = 0, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LGRAY, DGRAY, LBLUE, LGREEN, LCYAN, LRED, LMAGENTA, YELLOW, WHITE
};
constexpr uint8_t attr(uint8_t fg, uint8_t bg = BLACK) { return static_cast<uint8_t>((bg << 4) | fg); }

struct UiKey {
    int code = 0;        // an SDL keycode (SDLK_*); 0 when the window was closed
    int ch = 0;          // the character typed (ASCII), 0 for the other keys
    bool shift = false;
    bool quit = false;   // the window was closed
};

class TextScreen {
public:
    static constexpr int COLS = 80, ROWS = 25;

    explicit TextScreen(Window& window);
    // Screens made after this feed on the keys and save pictures as the next one does (for checking the screens made
    // deep inside the game unattended)
    static void defaultScript(const std::string& keys, const std::string& shotPrefix);

    // ---- drawing ----
    void clear(uint8_t a = attr(LGRAY));
    void put(int col, int row, const std::string& s, uint8_t a);
    void putCenter(int row, const std::string& s, uint8_t a);   // centered within the whole screen
    // the text of the original's menus: a backslash highlights the next character, \( and \) a span
    void putMarkup(int col, int row, const std::string& s, uint8_t normal, uint8_t highlight);
    void fill(int col, int row, int w, int h, uint8_t a, char c = ' ');
    // a single line frame with an optional title in its top border; the inside is cleared
    void box(int col, int row, int w, int h, uint8_t a, const std::string& title = "");
    void present();   // shows what has been drawn

    uint16_t cell(int col, int row) const { return m_cells[row * COLS + col]; }
    std::string rowText(int row) const;   // the characters of a row, for tests

    // ---- keys ----
    // Waits for a key (the scripted ones first); the screen is shown while waiting
    UiKey waitKey();
    // A key if there is one, without waiting
    bool pollKey(UiKey& k);
    // Makes the window report typed characters (for entering names)
    bool hasGamepad() const { return m_win.hasGamepad(); }
    void startText() { SDL_StartTextInput(); }
    void stopText() { SDL_StopTextInput(); }

    // The unattended runs: a list of keys such as "Down,Enter,x,Esc" and "text:Name" for typing
    void script(const std::string& keys);
    bool scripted() const { return m_scripted; }
    bool scriptDone() const { return m_scripted && m_script.empty(); }
    // Called when the script has run out and a key is wanted (to save a screenshot and quit)
    void onScriptEnd(std::function<void()> f) { m_scriptEnd = std::move(f); }

    // Saves the screen as <prefix>NN.bmp each time a key is asked for (for checking the screens unattended)
    void shotEveryKey(const std::string& prefix) { m_shotPrefix = prefix; }

    bool quit() const { return m_quit; }
    void setQuit() { m_quit = true; }
    Window& window() { return m_win; }
    bool save(const char* path);

private:
    Window& m_win;
    FrameBuffer m_fb;
    std::vector<uint16_t> m_cells;   // char | attribute << 8
    std::deque<UiKey> m_script;
    std::deque<UiKey> m_pending;     // keys read from the window but not given out yet
    bool m_scripted = false, m_quit = false;
    std::function<void()> m_scriptEnd;
    std::string m_shotPrefix;
    int m_shotCount = 0;
};

} // namespace corncob
