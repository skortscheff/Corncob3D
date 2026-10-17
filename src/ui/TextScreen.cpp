// TextScreen.cpp - An 80x25 colour text screen and its keyboard (see TextScreen.hpp)

#include "TextScreen.hpp"
#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <functional>

namespace corncob {

namespace {

// the 16 colours of the PC's text mode
constexpr Rgb PALETTE[16] = {{0, 0, 0},       {0, 0, 170},     {0, 170, 0},     {0, 170, 170}, {170, 0, 0},     {170, 0, 170},
                             {170, 85, 0},    {170, 170, 170}, {85, 85, 85},    {85, 85, 255}, {85, 255, 85},   {85, 255, 255},
                             {255, 85, 85},   {255, 85, 255},  {255, 255, 85},  {255, 255, 255}};

// the line drawing characters of the boxes, as codes in a cell
enum : char { L_H = 1, L_V, L_TL, L_TR, L_BL, L_BR };

constexpr int CELL_W = 8, CELL_H = 14;

UiKey keyFromToken(const std::string& tok) {
    struct Named { const char* name; int code; int ch; };
    static const Named names[] = {{"Enter", SDLK_RETURN, '\r'}, {"Esc", SDLK_ESCAPE, 0}, {"Up", SDLK_UP, 0},     {"Down", SDLK_DOWN, 0},
                                  {"Left", SDLK_LEFT, 0},       {"Right", SDLK_RIGHT, 0}, {"PgUp", SDLK_PAGEUP, 0}, {"PgDn", SDLK_PAGEDOWN, 0},
                                  {"BS", SDLK_BACKSPACE, '\b'}, {"Home", SDLK_HOME, 0},   {"End", SDLK_END, 0},     {"Space", SDLK_SPACE, ' '},
                                  {"Ins", SDLK_INSERT, 0},      {"F1", SDLK_F1, 0},       {"F2", SDLK_F2, 0},       {"Tab", SDLK_TAB, '\t'}};
    for (const Named& n : names)
        if (tok == n.name) return {n.code, n.ch, false, false};
    UiKey k;
    if (tok.size() == 1) {
        const char c = tok[0];
        k.ch = c;
        k.shift = std::isupper(static_cast<unsigned char>(c)) != 0;
        k.code = std::tolower(static_cast<unsigned char>(c));
    }
    return k;
}

} // namespace

namespace {
std::string g_defaultKeys, g_defaultShots;
}

void TextScreen::defaultScript(const std::string& keys, const std::string& shotPrefix) {
    g_defaultKeys = keys;
    g_defaultShots = shotPrefix;
}

TextScreen::TextScreen(Window& window)
    : m_win(window), m_fb(window.sdlRenderer(), COLS * CELL_W, ROWS * CELL_H), m_cells(COLS * ROWS, static_cast<uint16_t>(' ' | (0x07 << 8))) {
    if (!g_defaultKeys.empty()) script(g_defaultKeys);
    if (!g_defaultShots.empty()) shotEveryKey(g_defaultShots);
}

void TextScreen::clear(uint8_t a) {
    std::fill(m_cells.begin(), m_cells.end(), static_cast<uint16_t>(' ' | (a << 8)));
}

void TextScreen::put(int col, int row, const std::string& s, uint8_t a) {
    if (row < 0 || row >= ROWS) return;
    for (size_t i = 0; i < s.size(); i++) {
        const int c = col + static_cast<int>(i);
        if (c < 0 || c >= COLS) continue;
        m_cells[row * COLS + c] = static_cast<uint16_t>(static_cast<uint8_t>(s[i]) | (a << 8));
    }
}

void TextScreen::putCenter(int row, const std::string& s, uint8_t a) {
    put((COLS - static_cast<int>(s.size())) / 2, row, s, a);
}

void TextScreen::putMarkup(int col, int row, const std::string& s, uint8_t normal, uint8_t highlight) {
    bool span = false;
    int c = col;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            i++;
            if (s[i] == '(') { span = true; continue; }
            if (s[i] == ')') { span = false; continue; }
            put(c++, row, std::string(1, s[i]), highlight);   // the hot key
            continue;
        }
        put(c++, row, std::string(1, s[i]), span ? highlight : normal);
    }
}

void TextScreen::fill(int col, int row, int w, int h, uint8_t a, char ch) {
    for (int r = row; r < row + h; r++)
        for (int c = col; c < col + w; c++)
            if (r >= 0 && r < ROWS && c >= 0 && c < COLS) m_cells[r * COLS + c] = static_cast<uint16_t>(static_cast<uint8_t>(ch) | (a << 8));
}

void TextScreen::box(int col, int row, int w, int h, uint8_t a, const std::string& title) {
    fill(col, row, w, h, a);
    for (int c = col + 1; c < col + w - 1; c++) {
        put(c, row, std::string(1, static_cast<char>(L_H)), a);
        put(c, row + h - 1, std::string(1, static_cast<char>(L_H)), a);
    }
    for (int r = row + 1; r < row + h - 1; r++) {
        put(col, r, std::string(1, static_cast<char>(L_V)), a);
        put(col + w - 1, r, std::string(1, static_cast<char>(L_V)), a);
    }
    put(col, row, std::string(1, static_cast<char>(L_TL)), a);
    put(col + w - 1, row, std::string(1, static_cast<char>(L_TR)), a);
    put(col, row + h - 1, std::string(1, static_cast<char>(L_BL)), a);
    put(col + w - 1, row + h - 1, std::string(1, static_cast<char>(L_BR)), a);
    if (!title.empty()) put(col + 2, row, " " + title + " ", a);
}

std::string TextScreen::rowText(int row) const {
    std::string s;
    for (int c = 0; c < COLS; c++) {
        const char ch = static_cast<char>(m_cells[row * COLS + c] & 0xFF);
        s += (ch >= 32 && ch < 127) ? ch : (ch >= L_H && ch <= L_BR ? '+' : ' ');
    }
    return s;
}

void TextScreen::present() {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            const uint16_t v = m_cells[r * COLS + c];
            const char ch = static_cast<char>(v & 0xFF);
            const uint8_t a = static_cast<uint8_t>(v >> 8);
            const Rgb fg = PALETTE[a & 15], bg = PALETTE[(a >> 4) & 15];
            const int x0 = c * CELL_W, y0 = r * CELL_H;
            if (ch >= L_H && ch <= L_BR) {   // a line drawing character
                m_fb.fillRect(x0, y0, CELL_W, CELL_H, bg);
                const int mx = CELL_W / 2 - 1, my = CELL_H / 2 - 1;
                const bool left = ch == L_H || ch == L_TR || ch == L_BR, right = ch == L_H || ch == L_TL || ch == L_BL;
                const bool up = ch == L_V || ch == L_BL || ch == L_BR, down = ch == L_V || ch == L_TL || ch == L_TR;
                if (left) m_fb.fillRect(x0, y0 + my, mx + 2, 2, fg);
                if (right) m_fb.fillRect(x0 + mx, y0 + my, CELL_W - mx, 2, fg);
                if (up) m_fb.fillRect(x0 + mx, y0, 2, my + 2, fg);
                if (down) m_fb.fillRect(x0 + mx, y0 + my, 2, CELL_H - my, fg);
            } else {
                const char s[2] = {ch, 0};
                m_fb.text(c, r, s, fg, &bg, CELL_W, CELL_H);
            }
        }
    }
    m_fb.setBrightness(1.0f);
    m_fb.present();
}

bool TextScreen::save(const char* path) {
    present();
    return m_fb.save(path);
}

void TextScreen::script(const std::string& keys) {
    m_scripted = true;
    size_t i = 0;
    while (i < keys.size()) {
        size_t j = keys.find(',', i);
        if (j == std::string::npos) j = keys.size();
        const std::string tok = keys.substr(i, j - i);
        if (tok.rfind("text:", 0) == 0) {
            for (char c : tok.substr(5)) m_script.push_back(keyFromToken(std::string(1, c)));
        } else if (!tok.empty()) {
            m_script.push_back(keyFromToken(tok));
        }
        i = j + 1;
    }
}

bool TextScreen::pollKey(UiKey& k) {
    if (!m_script.empty()) {
        k = m_script.front();
        m_script.pop_front();
        return true;
    }
    if (m_pending.empty()) {
        if (!m_win.pollEvents()) {
            m_quit = true;
            k = UiKey{0, 0, false, true};
            return true;
        }
        const std::string& typed = m_win.typedText();
        size_t typedAt = 0;
        for (const Window::KeyPress& p : m_win.keyPresses()) {
            UiKey u;
            u.code = p.code;
            u.shift = p.shift;
            if (p.code == SDLK_RETURN || p.code == SDLK_KP_ENTER) u.ch = '\r';
            else if (p.code == SDLK_BACKSPACE) u.ch = '\b';
            else if (p.code == SDLK_ESCAPE) u.ch = 0;
            else if (p.code >= 32 && p.code < 127) {
                u.ch = p.code;
                if (p.shift && std::isalpha(p.code)) u.ch = std::toupper(p.code);
                // text input gives the real character (shifted digits, symbols); use it when present
                if (typedAt < typed.size() && static_cast<unsigned char>(typed[typedAt]) < 127) u.ch = typed[typedAt++];
            }
            m_pending.push_back(u);
        }
    }
    if (m_pending.empty()) return false;
    k = m_pending.front();
    m_pending.pop_front();
    return true;
}

UiKey TextScreen::waitKey() {
    present();
    if (!m_shotPrefix.empty()) {
        char path[512];
        std::snprintf(path, sizeof path, "%s%03d.bmp", m_shotPrefix.c_str(), m_shotCount++);
        m_fb.save(path);
    }
    for (;;) {
        UiKey k;
        if (m_scripted && m_script.empty() && m_pending.empty()) {   // the script is over
            if (m_scriptEnd) {
                auto f = m_scriptEnd;
                m_scriptEnd = nullptr;
                f();
            }
            m_quit = true;
            return UiKey{0, 0, false, true};
        }
        if (pollKey(k)) return k;
        SDL_Delay(10);
    }
}

} // namespace corncob
