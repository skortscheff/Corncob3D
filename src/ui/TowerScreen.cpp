// TowerScreen.cpp - The screens the game shows from inside a flight (see TowerScreen.hpp)

#include "TowerScreen.hpp"
#include "assets/Assets.hpp"
#include "ui/GameText.hpp"
#include "ui/TextScreen.hpp"
#include "ui/Widgets.hpp"
#include <SDL.h>
#include <algorithm>

namespace corncob {

using namespace ui;

TowerChoice runTowerInfo(Window& window, const TheaterInfo& theater, bool canGo) {
    TextScreen t(window);
    // the text, with where each mission starts
    std::vector<std::string> lines;
    std::vector<int> missionTop;   // the first line of each mission
    lines.push_back(" " + (theater.airfield.empty() ? std::string("Airfield") : theater.airfield));
    for (const std::string& l : theater.airfieldBanner) lines.push_back(l);
    for (const MissionInfo& m : theater.missions) {
        missionTop.push_back(static_cast<int>(lines.size()));
        lines.push_back("");
        lines.push_back(" Mission Code Name : " + m.codeName);
        lines.push_back(" Direction         : " + m.direction);
        lines.push_back(" Distance          : " + m.distance);
        lines.push_back(" Difficulty        : " + m.difficulty);
        for (const std::string& b : m.briefing) lines.push_back(b);
    }
    if (theater.missions.empty()) {
        lines.push_back("");
        lines.push_back("The intelligence officer has nothing for you at this airfield.");
    }
    const int visible = TextScreen::ROWS - 5;
    int sel = 0, top = 0;
    const int nm = static_cast<int>(theater.missions.size());
    const int maxTop = std::max(0, static_cast<int>(lines.size()) - visible);
    TowerChoice result;
    for (;;) {
        t.clear(DESKTOP);
        t.box(0, 0, TextScreen::COLS, TextScreen::ROWS - 1, WINDOW, "Intelligence Information");
        for (int i = 0; i < visible; i++) {
            const int li = top + i;
            if (li >= static_cast<int>(lines.size())) break;
            bool chosen = false;
            if (nm > 0 && canGo) {
                const int from = missionTop[sel];
                const int to = sel + 1 < nm ? missionTop[sel + 1] : static_cast<int>(lines.size());
                chosen = li >= from + 1 && li < from + 5 && li < to;   // the heading lines of the mission
            }
            const uint8_t a = chosen ? CHOSEN : WINDOW;
            t.fill(1, 2 + i, TextScreen::COLS - 2, 1, a);
            t.put(2, 2 + i, lines[li], a);
        }
        const std::string help = canGo && nm > 0 ? "Type UP/DOWN to choose a mission, PGUP, PGDN, <CR> to go there, <ESC> to cancel"
                                                 : "Type <UP>, <DOWN>, PGUP, PGDN, <CR> to return to the game...";
        t.put(2, TextScreen::ROWS - 2, help, WINDOW_TITLE);
        const UiKey k = t.waitKey();
        if (k.quit || k.code == SDLK_ESCAPE) return result;
        if (k.code == SDLK_RETURN || k.code == SDLK_KP_ENTER) {
            if (canGo && nm > 0) {
                const MissionInfo& m = theater.missions[sel];
                result.mission = true;
                result.x = m.x;
                result.y = m.y;
                result.z = m.z;
                result.yaw = m.yaw;
            }
            return result;
        }
        if (canGo && nm > 0 && (k.code == SDLK_DOWN || k.code == SDLK_UP)) {
            sel = k.code == SDLK_DOWN ? std::min(nm - 1, sel + 1) : std::max(0, sel - 1);
            top = std::max(0, std::min(maxTop, missionTop[sel] - 1));
        } else if (k.code == SDLK_DOWN) top = std::min(maxTop, top + 1);
        else if (k.code == SDLK_UP) top = std::max(0, top - 1);
        else if (k.code == SDLK_PAGEDOWN) top = std::min(maxTop, top + visible - 1);
        else if (k.code == SDLK_PAGEUP) top = std::max(0, top - (visible - 1));
    }
}

void runBossScreen(Window& window) {
    TextScreen t(window);
    t.clear(attr(LGRAY, BLACK));
    const std::vector<std::string> lines = readTextLines(assetPath("BOSS.TXT"));
    for (size_t i = 0; i < lines.size() && i < static_cast<size_t>(TextScreen::ROWS); i++) t.put(0, static_cast<int>(i), lines[i], attr(LGRAY, BLACK));
    t.waitKey();
}

void runTowerMessage(Window& window, const std::string& text) {
    TextScreen t(window);
    t.clear(attr(LGRAY, BLACK));
    t.put(0, 2, text, attr(LGRAY, BLACK));
    t.put(0, 4, "Type almost any key to continue...", attr(LGRAY, BLACK));
    t.waitKey();
}

} // namespace corncob
