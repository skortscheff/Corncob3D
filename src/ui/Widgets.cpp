// Widgets.cpp - The pieces the menu screens are made of (see Widgets.hpp)

#include "Widgets.hpp"
#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace corncob::ui {

char hotKey(const std::string& label) {
    for (size_t i = 0; i + 1 < label.size(); i++) {
        if (label[i] == '\\' && label[i + 1] != '(' && label[i + 1] != ')') return static_cast<char>(std::tolower(static_cast<unsigned char>(label[i + 1])));
    }
    return 0;
}

std::string plainLabel(const std::string& label) {
    std::string s;
    for (size_t i = 0; i < label.size(); i++) {
        if (label[i] == '\\' && i + 1 < label.size()) {
            i++;
            if (label[i] == '(' || label[i] == ')') continue;
        }
        s += label[i];
    }
    return s;
}

MenuResult runMenu(TextScreen& t, int col, int row, int width, const std::vector<MenuItem>& items, int& selected,
                   const std::function<void()>& redraw) {
    const int n = static_cast<int>(items.size());
    if (selected < 0 || selected >= n) selected = 0;
    for (;;) {
        redraw();
        for (int i = 0; i < n; i++) {
            const bool chosen = i == selected;
            t.fill(col, row + i, width, 1, chosen ? CHOSEN : WINDOW);
            t.putMarkup(col + 1, row + i, items[i].label, chosen ? CHOSEN : WINDOW, chosen ? CHOSEN_HOT : WINDOW_HOT);
        }
        const UiKey k = t.waitKey();
        if (k.quit) return {-1, true};
        if (k.code == SDLK_ESCAPE) return {-1, false};
        if (k.code == SDLK_RETURN || k.code == SDLK_KP_ENTER) return {items[selected].id, false};
        if (k.code == SDLK_UP) selected = (selected + n - 1) % n;
        else if (k.code == SDLK_DOWN) selected = (selected + 1) % n;
        else if (k.code == SDLK_HOME) selected = 0;
        else if (k.code == SDLK_END) selected = n - 1;
        else if (k.ch >= 32 && k.ch < 127) {
            const char c = static_cast<char>(std::tolower(k.ch));
            for (int i = 0; i < n; i++)
                if (hotKey(items[i].label) == c) {
                    selected = i;
                    return {items[i].id, false};
                }
        }
    }
}

Rect centeredBox(TextScreen& t, const std::string& title, const std::vector<std::string>& lines, int minWidth, int extraRows) {
    int w = std::max(minWidth, static_cast<int>(title.size()) + 6);
    for (const std::string& l : lines) w = std::max(w, static_cast<int>(plainLabel(l).size()) + 4);
    w = std::min(w, TextScreen::COLS - 2);
    const int h = static_cast<int>(lines.size()) + 2 + extraRows;
    const Rect r{(TextScreen::COLS - w) / 2, std::max(0, (TextScreen::ROWS - h) / 2), w, h};
    t.box(r.col, r.row, r.w, r.h, WINDOW, title);
    t.put(r.col + 2, r.row, " " + title + " ", WINDOW_TITLE);
    for (size_t i = 0; i < lines.size(); i++) t.putMarkup(r.col + 2, r.row + 1 + static_cast<int>(i), lines[i], WINDOW, WINDOW_HOT);
    return r;
}

bool messageBox(TextScreen& t, const std::string& title, const std::vector<std::string>& lines, const std::string& footer) {
    std::vector<std::string> all = lines;
    all.push_back("");
    all.push_back(footer);
    centeredBox(t, title, all);
    const UiKey k = t.waitKey();
    return !k.quit;
}

bool confirmBox(TextScreen& t, const std::string& title, const std::vector<std::string>& lines, char yes, bool enterIsYes) {
    centeredBox(t, title, lines);
    const UiKey k = t.waitKey();
    if (k.quit) return false;
    if (enterIsYes && (k.code == SDLK_RETURN || k.code == SDLK_KP_ENTER)) return true;
    return k.ch != 0 && std::tolower(k.ch) == yes;
}

// What a gamepad can type: up / down changes the last character through these, right adds one, left takes one away
static const char kPadChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -.";

bool inputLine(TextScreen& t, const std::string& title, const std::vector<std::string>& lines, std::string& value, size_t maxLen,
               const std::string& prompt) {
    std::string text = value;
    t.startText();
    for (;;) {
        std::vector<std::string> all = lines;
        all.push_back("");
        all.push_back(prompt + text + "_");
        if (t.hasGamepad()) all.push_back("Pad: up/down letter, right next, left delete, A done");
        centeredBox(t, title, all, std::max(static_cast<int>(prompt.size() + maxLen) + 6, t.hasGamepad() ? 56 : 0));
        const UiKey k = t.waitKey();
        if (k.quit || k.code == SDLK_ESCAPE) {
            t.stopText();
            return false;
        }
        if (k.code == SDLK_RETURN || k.code == SDLK_KP_ENTER) {
            t.stopText();
            value = text;
            return true;
        }
        if (k.code == SDLK_BACKSPACE || k.code == SDLK_LEFT) {
            if (!text.empty()) text.pop_back();
        } else if (k.code == SDLK_UP || k.code == SDLK_DOWN) {   // the gamepad's way to type
            const int n = static_cast<int>(sizeof kPadChars) - 1;
            if (text.empty()) {
                if (maxLen > 0) text += kPadChars[0];
            } else {
                const char* at = std::strchr(kPadChars, text.back());
                const int i = at ? static_cast<int>(at - kPadChars) : -1;
                text.back() = kPadChars[((i < 0 ? 0 : i) + (k.code == SDLK_UP ? 1 : n - 1)) % n];
            }
        } else if (k.code == SDLK_RIGHT) {
            if (text.size() < maxLen) text += text.empty() ? 'A' : text.back();
        } else if (k.ch >= 32 && k.ch < 127 && text.size() < maxLen) {
            text += static_cast<char>(k.ch);
        }
    }
}

bool inputNumber(TextScreen& t, const std::string& title, const std::vector<std::string>& lines, int& value, int lo, int hi) {
    std::string text = std::to_string(value);
    t.startText();
    for (;;) {
        std::vector<std::string> all = lines;
        all.push_back("");
        all.push_back("Enter new value [" + std::to_string(lo) + "-" + std::to_string(hi) + "]  " + text + "_");
        centeredBox(t, title, all, 50);
        const UiKey k = t.waitKey();
        if (k.quit || k.code == SDLK_ESCAPE) {
            t.stopText();
            return false;
        }
        if (k.code == SDLK_RETURN || k.code == SDLK_KP_ENTER) {
            const int v = text.empty() ? -1 : std::atoi(text.c_str());
            if (v >= lo && v <= hi) {
                t.stopText();
                value = v;
                return true;
            }
            text.clear();   // not in range: ask again
        } else if (k.code == SDLK_BACKSPACE) {
            if (!text.empty()) text.pop_back();
        } else if (k.code == SDLK_UP || k.code == SDLK_DOWN || k.code == SDLK_LEFT || k.code == SDLK_RIGHT) {   // gamepad
            const int step = (k.code == SDLK_UP || k.code == SDLK_RIGHT) ? 1 : -1;
            const int big = (k.code == SDLK_LEFT || k.code == SDLK_RIGHT) ? 10 : 1;
            const int cur = text.empty() ? lo : std::atoi(text.c_str());
            text = std::to_string(std::max(lo, std::min(hi, cur + step * big)));
        } else if (k.ch >= '0' && k.ch <= '9' && text.size() < 6) {
            text += static_cast<char>(k.ch);
        }
    }
}

UiKey pager(TextScreen& t, const std::string& title, const std::vector<std::string>& lines, const std::string& footer) {
    const int col = 0, row = 0, w = TextScreen::COLS, h = TextScreen::ROWS;
    const int visible = h - 3;
    int top = 0;
    const int maxTop = std::max(0, static_cast<int>(lines.size()) - visible);
    for (;;) {
        t.box(col, row, w, h, WINDOW, title);
        t.put(col + 2, row, " " + title + " ", WINDOW_TITLE);
        for (int i = 0; i < visible; i++) {
            const int li = top + i;
            if (li >= static_cast<int>(lines.size())) break;
            t.putMarkup(col + 1, row + 1 + i, lines[li], WINDOW, WINDOW_HOT);
        }
        t.put(col + 1, row + h - 2, footer, WINDOW_TITLE);
        const UiKey k = t.waitKey();
        if (k.quit) return k;
        if (k.code == SDLK_ESCAPE || k.code == SDLK_RETURN || k.code == SDLK_KP_ENTER) return k;
        if (k.code == SDLK_DOWN) top = std::min(maxTop, top + 1);
        else if (k.code == SDLK_UP) top = std::max(0, top - 1);
        else if (k.code == SDLK_PAGEDOWN) top = std::min(maxTop, top + visible - 1);
        else if (k.code == SDLK_PAGEUP) top = std::max(0, top - (visible - 1));
        else if (k.code == SDLK_HOME) top = 0;
        else if (k.code == SDLK_END) top = maxTop;
        else if (k.ch >= 32 && k.ch < 127) return k;   // a letter or number: the caller may want it (chapters)
    }
}

} // namespace corncob::ui
