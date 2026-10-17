// Widgets.hpp - The pieces the menu screens are made of: menus, message boxes, line entry, paged text
//
// Colours (text attributes): windows are black on light grey with a red hot key, the chosen menu line
// black on cyan, the desktop light grey on blue.

#pragma once
#include "ui/TextScreen.hpp"
#include <functional>
#include <string>
#include <vector>

namespace corncob::ui {

constexpr uint8_t DESKTOP = attr(LGRAY, BLUE);
constexpr uint8_t WINDOW = attr(BLACK, LGRAY);
constexpr uint8_t WINDOW_HOT = attr(RED, LGRAY);
constexpr uint8_t WINDOW_TITLE = attr(WHITE, LGRAY);
constexpr uint8_t CHOSEN = attr(BLACK, CYAN);
constexpr uint8_t CHOSEN_HOT = attr(RED, CYAN);
constexpr uint8_t BAND = attr(YELLOW, BLUE);
constexpr uint8_t PLAIN = attr(WHITE, BLUE);
constexpr uint8_t DIM = attr(LCYAN, BLUE);

struct MenuItem {
    std::string label;   // "\Duty roster": the backslash marks the hot key
    int id;
    bool enabled = true;
};

// The hot key of a label (lower case), 0 when it has none
char hotKey(const std::string& label);
// The label without its backslash marks
std::string plainLabel(const std::string& label);

struct MenuResult {
    int id = -1;          // the item chosen, -1 for escape
    bool closed = false;  // the window was closed
};

// Runs a menu until an item is chosen. `redraw` paints the background (the screen, the window around the
// items) before the items are drawn; `selected` is the index of the chosen line and is kept for the next time.
MenuResult runMenu(TextScreen& t, int col, int row, int width, const std::vector<MenuItem>& items, int& selected,
                   const std::function<void()>& redraw);

// A window with some lines of text, closed by a key. Returns false when the window was closed.
bool messageBox(TextScreen& t, const std::string& title, const std::vector<std::string>& lines,
                const std::string& footer = "Type almost any key to continue...");
// Asks yes or no: the key of the yes answer is `yes` (a lower case letter), Enter also counts when
// enterIsYes. Anything else is no.
bool confirmBox(TextScreen& t, const std::string& title, const std::vector<std::string>& lines, char yes, bool enterIsYes);
// Asks for a line of text; false when escape was pressed or the window closed
bool inputLine(TextScreen& t, const std::string& title, const std::vector<std::string>& lines, std::string& value,
               size_t maxLen, const std::string& prompt = "");
// Asks for a number in a range; false when escape was pressed
bool inputNumber(TextScreen& t, const std::string& title, const std::vector<std::string>& lines, int& value, int lo, int hi);

// Text in a window that can be paged through: arrow keys, page keys, Home and End; escape or enter leaves.
// The lines may carry the backslash marks of putMarkup. Returns the key that ended it (the code is 0 when
// the window was closed).
UiKey pager(TextScreen& t, const std::string& title, const std::vector<std::string>& lines,
            const std::string& footer = "Type PGUP, PGDN, <CR> to return...");

// Centers `lines` in a window of its own and returns the window's rectangle
struct Rect { int col, row, w, h; };
Rect centeredBox(TextScreen& t, const std::string& title, const std::vector<std::string>& lines, int minWidth = 0, int extraRows = 0);

} // namespace corncob::ui
