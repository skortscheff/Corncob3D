// FrontEnd.cpp - The menus (see FrontEnd.hpp)

#include "FrontEnd.hpp"
#include "ui/ControlSetup.hpp"
#include "assets/Assets.hpp"
#include "game/Keyboard.hpp"
#include "game/TheaterFiles.hpp"
#include "ui/Career.hpp"
#include "ui/GameText.hpp"
#include "ui/Widgets.hpp"
#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

namespace corncob {

using namespace ui;

namespace {

std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char* f, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

std::string plural(int n, const char* one = "", const char* many = "s") { return n == 1 ? one : many; }

std::string padRight(std::string s, size_t n) {
    if (s.size() < n) s.append(n - s.size(), ' ');
    return s.substr(0, std::max(n, static_cast<size_t>(0)));
}

std::string padLeft(const std::string& s, size_t n) { return s.size() >= n ? s : std::string(n - s.size(), ' ') + s; }

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::vector<uint8_t> readBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool writeBytes(const std::string& path, const std::vector<uint8_t>& b) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
    return static_cast<bool>(out);
}

// the wickedness 1-8 (0 random) the way the menus say it
std::string wickedText(int w) { return w == 0 ? "random spread of strength" : fmt("strength %d", w); }

// -1 for a random spread, else the quality 0-7 of a wickedness of 1-8
int qualityOf(int wicked) { return wicked <= 0 ? -1 : std::min(7, wicked - 1); }

// flight time as h:mm:ss
std::string flightTime(long seconds) { return fmt("%ld:%02ld:%02ld", seconds / 3600, (seconds / 60) % 60, seconds % 60); }

} // namespace

struct SetupMenus {
    TextScreen& t;
    Settings& s;
    int kbSel = 0;
    bool inMission = false;   // opened from a flight: the way out is back to the game or to the main menu

    bool closed() const { return t.quit(); }

    // ---- looks ----
    void header() {
        t.clear(DESKTOP);
        t.putCenter(0, "Corncob 3D   Version 1.0", BAND);
        t.putCenter(1, "An MVP Software Production", DIM);
        t.putCenter(2, "Copyright 1991,1992   Pie in the Sky Software.", DIM);
    }

    std::string controlText() const {
        if (!s.joystick) return "Keyboard";
        return std::string("Keyboard, and a gamepad when there is one") + (s.invertPitch ? " (pitch inverted)" : "");
    }

    // ---- information ----
    void info(Info which, const std::string& title) {
        std::vector<std::string> lines = infoText(which);
        if (lines.empty()) lines.push_back("There is no information about this menu.");
        pager(t, title, lines, "Type PGUP, PGDN, <CR> to return...");
    }

    // ---- control setup ----
    enum { K_KEYBOARD = 1, K_JOYSTICK, K_INVERT, K_KEYS, K_HD, K_SMOOTH, K_FPS, K_PANEL, K_INFO, K_RETURN, K_MAIN };

    // the actions whose keys can be chosen, with the DOS scan code the game reads for each
    struct Action { const char* name; uint8_t dos; };
    static const std::vector<Action>& actions() {
        static const std::vector<Action> a = {
            {"Nose down", 0x48}, {"Nose up", 0x50}, {"Roll left", 0x4B}, {"Roll right", 0x4D}, {"Rudder left", 0x2C}, {"Rudder right", 0x2D},
            {"Throttle up", 0x4E}, {"Throttle down", 0x4A}, {"Brakes", 0x34}, {"Guns", 0x39}, {"Missiles", 0x2E}, {"Drop bomb", 0x30},
            {"Flaps", 0x21}, {"Autopilot", 0x1E}, {"Eject / step out", 0x12}, {"Rocket booster", 0x17}, {"Map mode", 0x32}, {"Pause", 0x19},
            {"Rear window", 0x13}, {"Ground detail dots", 0x22}, {"Advance time", 0x14}, {"Sound toggle", 0x1F}};
        return a;
    }
    static std::string keyName(const Settings& s, uint8_t dos) {
        for (const auto& b : s.bindings)
            if (b.first == dos) return SDL_GetScancodeName(static_cast<SDL_Scancode>(b.second));
        return "(original key)";
    }
    void applyBindings() {
        std::vector<std::pair<int, uint8_t>> v;
        for (const auto& b : s.bindings) v.push_back({b.second, static_cast<uint8_t>(b.first)});
        DosKeyboard::setBindings(v);
    }
    void redefineKeys() {
        int sel = 0;
        for (;;) {
            std::vector<MenuItem> items;
            for (size_t i = 0; i < actions().size(); i++)
                items.push_back({padRight(actions()[i].name, 22) + keyName(s, actions()[i].dos), static_cast<int>(i)});
            items.push_back({"\\Reset all keys to the original ones", 100});
            items.push_back({"\\Return to previous menu", 101});
            const MenuResult r = runMenu(t, 4, 5, 58, items, sel, [&] {
                header();
                t.box(3, 3, 62, 22, WINDOW, "Define keys");
                t.put(5, 4, "Choose an action, <CR>, then press the key you want for it", WINDOW);
            });
            if (r.closed || r.id == -1 || r.id == 101) return;
            if (r.id == 100) {
                s.bindings.clear();
            } else {
                const Action& a = actions()[r.id];
                centeredBox(t, a.name, {std::string("Press the key for ") + a.name + ",", "or <ESC> to keep the present one."});
                const UiKey k = t.waitKey();
                if (k.quit) return;
                if (k.code != SDLK_ESCAPE && k.code != 0) {
                    const int sc = static_cast<int>(SDL_GetScancodeFromKey(k.code));
                    if (sc != 0) {
                        s.bindings.erase(std::remove_if(s.bindings.begin(), s.bindings.end(), [&](const std::pair<int, int>& b) { return b.first == a.dos || b.second == sc; }), s.bindings.end());
                        s.bindings.push_back({a.dos, sc});
                    }
                }
            }
            applyBindings();
            saveSettings(s);
        }
    }
    // true: the player chose to leave the mission for the main menu
    bool controlSetup() {
        while (!closed()) {
            std::vector<MenuItem> items = {
                {"choose \\Keyboard control", K_KEYBOARD},
                {"choose \\Joystick (gamepad) control too", K_JOYSTICK},
                {std::string(s.invertPitch ? "stop in\\Verting" : "in\\Vert") + " the gamepad pitch", K_INVERT},
                {"re\\Define keys", K_KEYS},
                {fmt("\\High resolution: %dx  (640x350 times %d)", s.hd, s.hd), K_HD},
                {std::string("\\Smoothing (anti-aliasing): ") + (s.smooth ? "on" : "off"), K_SMOOTH},
                {fmt("\\Frame rate: %d frames per second", s.fps), K_FPS},
                {std::string("instrument \\Panel: ") + (s.modernPanel ? "modern" : "original"), K_PANEL},
                {"get \\Information about this menu", K_INFO},
                {inMission ? "\\Return to the game" : "\\Return to previous menu", K_RETURN},
            };
            if (inMission) items.push_back({"Return to the \\Main menu", K_MAIN});
            const int extra = static_cast<int>(items.size()) - 10;   // rows below the menu move down
            const MenuResult r = runMenu(t, 4, 9, 50, items, kbSel, [&] {
                header();
                t.box(3, 4, 54, 19 + extra, WINDOW, "Keyboard/joystick setup");
                t.put(5, 5, "Use arrow keys or type highlighted", WINDOW);
                t.put(5, 6, "letter to choose action:", WINDOW);
                t.put(5, 19 + extra, "Current choices:", WINDOW);
                t.put(5, 20 + extra, controlText() + " control.", WINDOW);
                t.put(5, 21 + extra, "See the Keyboard Play chapter of the documentation.", WINDOW);
            });
            if (r.closed || r.id == -1 || r.id == K_RETURN) return false;
            if (r.id == K_MAIN) {
                if (confirmBox(t, "Leave the Mission", {"Abandon this mission and return to the main menu?", "", "Type <ENTER> to leave, almost anything else to stay..."}, '\0', true)) {
                    saveSettings(s);
                    return true;
                }
                continue;
            }
            if (r.id == K_KEYBOARD) s.joystick = false;
            else if (r.id == K_JOYSTICK) s.joystick = true;
            else if (r.id == K_INVERT) s.invertPitch = !s.invertPitch;
            else if (r.id == K_KEYS) redefineKeys();
            else if (r.id == K_HD) s.hd = s.hd % 4 + 1;
            else if (r.id == K_SMOOTH) s.smooth = !s.smooth;
            else if (r.id == K_PANEL) s.modernPanel = !s.modernPanel;
            else if (r.id == K_FPS) s.fps = s.fps >= 60 ? 20 : 60;   // the original's 20, or the 60 Hz engine
            else if (r.id == K_INFO) info(Info::Keyboard, "Keyboard/Joystick Menu Information");
            saveSettings(s);
        }
        return false;
    }
};

struct FrontEnd::Impl : SetupMenus {
    MusicBox* music;
    MissionLauncher launch;
    Career career;
    Pilot trainee;
    std::string defDir;
    std::vector<std::string> theaterNames;
    std::vector<TheaterInfo> theaterInfos;
    bool done = false;
    bool toMain = false;   // a mission was left for the main menu: the menus above it close
    int mainSel = 0, trainSel = 0, combatSel = 0, dutySel = 0, hiSel = 0;

    Impl(TextScreen& screen, Settings& settings, MusicBox* m, MissionLauncher l)
        : SetupMenus{screen, settings}, music(m), launch(std::move(l)), career(saveDirectory()) {
        career.load();
        defDir = assetPath("DEF/");
        theaterNames = listTheaters(defDir);
        for (const std::string& n : theaterNames) {
            TheaterInfo info;
            loadTheaterInfo(defDir + n + ".TWR", info);
            info.file = n;
            theaterInfos.push_back(info);
        }
        std::vector<std::string> required;
        for (const std::string& n : theaterNames) required.push_back(n);
        career.setRequiredTheaters(required);
        trainee.name = "trainee";
        trainee.id = 0;
    }

    ~Impl() {
        for (const TheaterRecord& th : trainee.theaters) std::filesystem::remove(career.theaterFile(th));   // the trainee leaves no trace
    }

    Pilot& pilot() { return career.active() ? *career.active() : trainee; }
    bool isTrainee() { return career.active() == nullptr; }

    std::string theaterTitle(const TheaterRecord& th) const {
        for (const TheaterInfo& i : theaterInfos)
            if (i.file == th.def) return i.name;
        return th.def;
    }


    // ---- the lists you choose from ----
    int chooseFromList(const std::string& title, const std::string& prompt, const std::vector<std::string>& entries, int selected = 0) {
        if (entries.empty()) return -1;
        int sel = std::max(0, std::min(selected, static_cast<int>(entries.size()) - 1));
        int top = 0;
        const int visible = 12;
        int w = static_cast<int>(prompt.size()) + 4;
        for (const std::string& e : entries) w = std::max(w, static_cast<int>(e.size()) + 6);
        w = std::min(w, 76);
        for (;;) {
            const int h = std::min(visible, static_cast<int>(entries.size())) + 5;
            const int col = (TextScreen::COLS - w) / 2, row = (TextScreen::ROWS - h) / 2;
            t.box(col, row, w, h, WINDOW, title);
            t.put(col + 2, row, " " + title + " ", WINDOW_TITLE);
            t.put(col + 2, row + 1, prompt, WINDOW);
            if (sel < top) top = sel;
            if (sel >= top + visible) top = sel - visible + 1;
            for (int i = 0; i < visible && top + i < static_cast<int>(entries.size()); i++) {
                const bool chosen = top + i == sel;
                t.fill(col + 1, row + 3 + i, w - 2, 1, chosen ? CHOSEN : WINDOW);
                t.put(col + 2, row + 3 + i, entries[top + i], chosen ? CHOSEN : WINDOW);
            }
            const UiKey k = t.waitKey();
            if (k.quit) return -1;
            if (k.code == SDLK_ESCAPE || k.code == SDLK_BACKSPACE) return -1;
            if (k.code == SDLK_RETURN || k.code == SDLK_KP_ENTER) return sel;
            if (k.code == SDLK_UP) sel = (sel + static_cast<int>(entries.size()) - 1) % static_cast<int>(entries.size());
            else if (k.code == SDLK_DOWN) sel = (sel + 1) % static_cast<int>(entries.size());
            else if (k.code == SDLK_HOME) sel = 0;
            else if (k.code == SDLK_END) sel = static_cast<int>(entries.size()) - 1;
            else if (k.code == SDLK_PAGEDOWN) sel = std::min(static_cast<int>(entries.size()) - 1, sel + visible);
            else if (k.code == SDLK_PAGEUP) sel = std::max(0, sel - visible);
        }
    }

    // ---- main menu ----
    enum { M_DUTY = 1, M_COMBAT, M_TRAIN, M_SCORES, M_READ, M_CONTROL, M_MUSIC, M_DOC, M_INFO, M_EXIT };

    void drawMain() {
        header();
        t.box(3, 4, 48, 14, WINDOW, "Main Menu");
        t.put(5, 5, "Use arrow keys or type highlighted", WINDOW);
        t.put(5, 6, "letter to choose action:", WINDOW);
        t.box(3, 19, 74, 5, WINDOW, "Current choices");
        const Pilot& p = career.active() ? *career.active() : trainee;
        std::string who = p.name;
        if (career.active()) who = std::string(rankName(p.rank, true)) + " " + p.name;
        t.put(5, 20, "Active pilot:  " + who, WINDOW);
        const Pilot& cp = p;
        std::string where = "none";
        if (cp.current >= 0 && cp.current < static_cast<int>(cp.theaters.size())) where = theaterTitle(cp.theaters[cp.current]);
        t.put(5, 21, "Theater-of-Operations:  " + where, WINDOW);
        t.put(5, 22, "Control:  " + controlText(), WINDOW);
        t.put(45, 20, std::string("Music  ") + (music && music->available() ? (s.music ? "On" : "Off") : "No Sound Card"), WINDOW);
        // the pilot, to the right of the menu
        t.box(53, 4, 24, 14, WINDOW, "Pilot");
        if (career.active()) {
            t.put(55, 5, rankName(p.rank, true), WINDOW);
            t.put(55, 6, p.name, WINDOW_TITLE);
            t.put(55, 8, fmt("%d mission%s", p.career.missions, plural(p.career.missions).c_str()), WINDOW);
            t.put(55, 9, fmt("%ld points", p.career.points), WINDOW);
            if (p.resurrections) t.put(55, 10, fmt("%d resurrection%s", p.resurrections, plural(p.resurrections).c_str()), WINDOW);
            const int medals = static_cast<int>(__builtin_popcount(p.medals));
            t.put(55, 12, fmt("%d decoration%s", medals, plural(medals).c_str()), WINDOW);
        } else {
            t.put(55, 5, "Trainee", WINDOW);
            t.put(55, 7, "No record is kept.", WINDOW);
            t.put(55, 8, "Create a pilot in the", WINDOW);
            t.put(55, 9, "duty roster to build", WINDOW);
            t.put(55, 10, "a career.", WINDOW);
        }
    }

    void mainMenu() {
        const std::vector<MenuItem> items = {
            {"\\Duty roster", M_DUTY},
            {"\\Combat missions", M_COMBAT},
            {"\\Training missions", M_TRAIN},
            {"display \\High score list", M_SCORES},
            {"\\Read This!!!", M_READ},
            {"keyboard/\\Joystick setup", M_CONTROL},
            {"\\Music toggle", M_MUSIC},
            {"\\View the Corncob 3D documentation", M_DOC},
            {"get \\Information about this menu", M_INFO},
            {"e\\Xit Corncob 3D", M_EXIT},
        };
        while (!done && !closed()) {
            toMain = false;
            const MenuResult r = runMenu(t, 4, 8, 46, items, mainSel, [&] { drawMain(); });
            if (r.closed) return;
            switch (r.id) {
            case M_DUTY: dutyRoster(); break;
            case M_COMBAT: combatMissions(); break;
            case M_TRAIN: trainingMissions(); break;
            case M_SCORES: highScores(); break;
            case M_READ: pager(t, "Read This!!!", infoText(Info::ReadThis), "Type PGUP, PGDN, <ESC> to return to the main menu..."); break;
            case M_CONTROL: controlSetup(); break;
            case M_MUSIC:
                s.music = !s.music;
                if (music) music->setOn(s.music);
                saveSettings(s);
                break;
            case M_DOC: documentation(); break;
            case M_INFO: info(Info::Main, "Information about Main Menu"); break;
            case M_EXIT:
            case -1: done = true; break;
            }
        }
    }

    // ---- documentation ----
    void documentation() {
        const std::vector<std::vector<std::string>> pages = docPages();
        if (pages.empty()) {
            messageBox(t, "Documentation", {"CORNCOB.DOC is missing."});
            return;
        }
        int page = 0;
        for (;;) {
            const std::string title = page == 0 ? "Corncob 3D documentation" : fmt("Corncob 3D documentation: chapter %d of %d", page, static_cast<int>(pages.size()) - 1);
            const std::string footer = "PGUP PGDN, chapter number, <CR> next chapter, b beginning, <ESC> exit";
            const UiKey k = pager(t, title, pages[page], footer);
            if (k.quit || k.code == SDLK_ESCAPE) return;
            if (k.code == SDLK_RETURN || k.code == SDLK_KP_ENTER) {
                if (page + 1 >= static_cast<int>(pages.size())) return;
                page++;
            } else if (k.ch == 'b' || k.ch == 'B') {
                page = 0;
            } else if (k.ch >= '0' && k.ch <= '9') {   // a chapter number
                std::string digits(1, static_cast<char>(k.ch));
                t.startText();
                for (;;) {
                    t.put(2, TextScreen::ROWS - 2, padRight("Enter a Chapter number:  " + digits + "_", 40), WINDOW_TITLE);
                    const UiKey d = t.waitKey();
                    if (d.quit || d.code == SDLK_ESCAPE) { digits.clear(); break; }
                    if (d.code == SDLK_RETURN || d.code == SDLK_KP_ENTER) break;
                    if (d.code == SDLK_BACKSPACE && !digits.empty()) digits.pop_back();
                    else if (d.ch >= '0' && d.ch <= '9' && digits.size() < 3) digits += static_cast<char>(d.ch);
                }
                t.stopText();
                if (!digits.empty()) {
                    const int n = std::atoi(digits.c_str());
                    if (n >= 1 && n < static_cast<int>(pages.size())) page = n;
                }
            }
        }
    }

    // ---- training missions ----
    enum { T_FLY = 1, T_WICKED, T_NUMBER, T_DEFAULT, T_PLANE, T_INFO, T_RETURN };
    void trainingMissions() {
        while (!closed()) {
            const TrainingSetup& tr = s.training;
            const std::vector<MenuItem> items = {
                {"\\Fly mission", T_FLY},
                {"modify \\Wickedness of enemies", T_WICKED},
                {"choose \\Number of enemies", T_NUMBER},
                {"choose \\Default values", T_DEFAULT},
                {"toggle invulnerable \\Plane", T_PLANE},
                {"get \\Information about this menu", T_INFO},
                {"\\Return to previous menu", T_RETURN},
            };
            const MenuResult r = runMenu(t, 4, 9, 46, items, trainSel, [&] {
                header();
                t.box(3, 4, 48, 12, WINDOW, "Training Missions");
                t.put(5, 5, "Use arrow keys or type highlighted", WINDOW);
                t.put(5, 6, "    letter to choose action:", WINDOW);
                t.box(3, 16, 74, 8, WINDOW, "Current choices");
                t.put(5, 17, std::string("Plane:  ") + (tr.invulnerable ? "Invulnerable" : "Standard"), WINDOW);
                t.put(5, 18, fmt("%d missile sites", tr.kla) + "  -  " + wickedText(tr.klaWicked), WINDOW);
                t.put(5, 19, fmt("%d AAA sites", tr.aaa) + "  -  " + wickedText(tr.aaaWicked), WINDOW);
                t.put(5, 20, fmt("%d ground vehicle sites", tr.vehicles) + "  -  " + wickedText(tr.vehWicked), WINDOW);
                t.put(5, 22, "Pilot:  " + (career.active() ? career.active()->name : std::string("trainee")), WINDOW);
                t.box(53, 4, 24, 12, WINDOW, "Pilot");
                t.put(55, 5, career.active() ? career.active()->name : "trainee", WINDOW);
            });
            if (r.closed || r.id == -1 || r.id == T_RETURN) return;
            switch (r.id) {
            case T_FLY: flyTraining(); break;
            case T_WICKED: trainingWickedness(); break;
            case T_NUMBER: trainingNumbers(); break;
            case T_DEFAULT:
                s.training.defaults();
                messageBox(t, "Values Set to Default", {"5 missile sites, a random spread of wickedness", "6 AAA sites, a random spread of wickedness",
                                                        "2 ground vehicle sites of wickedness 4"});
                break;
            case T_PLANE: s.training.invulnerable = !s.training.invulnerable; break;
            case T_INFO: info(Info::Training, "Training Missions Information"); break;
            }
            saveSettings(s);
            if (toMain) return;
        }
    }

    void trainingWickedness() {
        const std::vector<MenuItem> items = {{"modify enemy \\Missile wickedness", 1}, {"modify enemy \\Ground vehicle", 2}, {"modify \\AAA wickedness", 3},
                                             {"\\Return to previous menu", 4}};
        int sel = 0;
        for (;;) {
            const MenuResult r = runMenu(t, 22, 11, 38, items, sel, [&] {
                header();
                t.box(20, 8, 42, 8, WINDOW, "Modify Wickedness of Enemies");
                t.put(22, 9, "Wickedness is 1-8 (8 is deadly), 0 a random spread", WINDOW);
            });
            if (r.closed || r.id == -1 || r.id == 4) return;
            int* w = r.id == 1 ? &s.training.klaWicked : r.id == 2 ? &s.training.vehWicked : &s.training.aaaWicked;
            const char* what = r.id == 1 ? "Missile" : r.id == 2 ? "Ground-Vehicle" : "AAA";
            int v = *w;
            if (inputNumber(t, fmt("Modify %s Wickedness", what),
                            {fmt("%s wickedness is currently %d.", what, *w), "Values of 1-8 mean increasing wickedness.", "A value of 0 will choose a random", "spread of wickedness."}, v, 0, 8))
                *w = v;
            if (closed()) return;
        }
    }

    void trainingNumbers() {
        const std::vector<MenuItem> items = {{"change number of \\Missile sites", 1}, {"change number of \\Ground-Vehicle sites", 2},
                                             {"change number of \\AAA sites", 3}, {"\\Return to previous menu", 4}};
        int sel = 0;
        for (;;) {
            const MenuResult r = runMenu(t, 22, 11, 42, items, sel, [&] {
                header();
                t.box(20, 8, 46, 8, WINDOW, "Choose Number of Enemies");
                t.put(22, 9, "The maximum of each is 25.", WINDOW);
            });
            if (r.closed || r.id == -1 || r.id == 4) return;
            int* n = r.id == 1 ? &s.training.kla : r.id == 2 ? &s.training.vehicles : &s.training.aaa;
            const char* what = r.id == 1 ? "Missile" : r.id == 2 ? "Ground-Vehicle" : "AAA";
            int v = *n;
            if (inputNumber(t, fmt("Choose # of %s Sites", what),
                            {fmt("The number of %s sites is currently %d.", what, *n), "The maximum number of sites is 25."}, v, 0, 25))
                *n = v;
            if (closed()) return;
        }
    }

    // the briefing before a training mission
    bool trainingBriefing() {
        const TrainingSetup& tr = s.training;
        std::vector<std::string> lines = {
            "Training Mission:",
            "",
            "Your objective is to destroy the flying saucer and to take out",
            "as many enemy fuel dumps and ground sites as possible.",
            "",
            "Latest intelligence reports enemy strengths as follows:",
            ""};
        lines.push_back(fmt("  %d Missile site%s, %s", tr.kla, plural(tr.kla).c_str(), wickedText(tr.klaWicked).c_str()));
        lines.push_back(fmt("  %d AAA site%s, %s", tr.aaa, plural(tr.aaa).c_str(), wickedText(tr.aaaWicked).c_str()));
        lines.push_back(fmt("  %d Ground-Vehicle site%s, %s", tr.vehicles, plural(tr.vehicles).c_str(), wickedText(tr.vehWicked).c_str()));
        lines.push_back("  Enemy Saucers");
        lines.push_back("");
        lines.push_back("(Note:) Training missions are much harder than the combat missions.");
        if (tr.invulnerable) {
            lines.push_back("");
            lines.push_back("This mission will not be recorded because an invulnerable plane is flown.");
        }
        lines.push_back("");
        lines.push_back("Type <CR> to continue, <ESC> to abort");
        header();
        const std::string who = std::string(career.active() ? rankName(pilot().rank, true) : "Trainee") + " " + pilot().name;
        centeredBox(t, "Mission Briefing", [&] {
            std::vector<std::string> all = {who + ", prepare for your mission!", ""};
            all.insert(all.end(), lines.begin(), lines.end());
            return all;
        }());
        const UiKey k = t.waitKey();
        return !k.quit && k.code != SDLK_ESCAPE;
    }

    void flyTraining() {
        if (!trainingBriefing()) return;
        const TrainingSetup& tr = s.training;
        MissionRequest rq;
        rq.training = true;
        rq.kla = tr.kla;
        rq.aaa = tr.aaa;
        rq.vehicles = tr.vehicles;
        rq.klaQuality = qualityOf(tr.klaWicked);
        rq.aaaQuality = qualityOf(tr.aaaWicked);
        rq.vehQuality = qualityOf(tr.vehWicked);
        rq.invincible = tr.invulnerable;
        rq.beginnerModel = s.beginnerModel;
        rq.comments = s.comments;
        rq.useGamepad = s.joystick;
        rq.invertPitch = s.invertPitch;
        rq.hd = s.hd;
        rq.smooth = s.smooth;
        rq.fps = s.fps;
        rq.modernPanel = s.modernPanel;
        fly(rq, nullptr, "Training Mission");
    }

    // ---- flying: the mission, and what comes after ----
    void fly(const MissionRequest& rq, TheaterRecord* th, const std::string& theaterName) {
        if (music) music->close();   // the game opens the sound card for itself
        const MissionOutcome out = launch(rq);
        if (music) {
            music->reopen();
            if (s.music) music->playMenu();
        }
        if (out.windowClosed) {
            t.setQuit();
            return;
        }
        if (out.toMainMenu) toMain = true;
        if (!out.over) return;
        afterMission(rq, th, theaterName, out);
    }

    void afterMission(const MissionRequest& rq, TheaterRecord* th, const std::string& theaterName, const MissionOutcome& out) {
        ResultBlock r;
        if (!ResultBlock::parse(out.result, r)) {
            messageBox(t, "Mission report", {"Mission report not available."});
            return;
        }
        Pilot& p = pilot();
        const bool counts = !rq.invincible && !(r.status & ST_ABORT_CTRL_C);
        // the history of a combat mission: what was destroyed stays destroyed
        int remaining = th ? th->objectivesRemaining : 0;
        if (th && counts && !out.universe.empty()) {
            writeBytes(career.theaterFile(*th), out.universe);
            UniverseSummary sum;
            if (summarizeUniverse(out.universe, sum)) remaining = sum.objectivesRemaining;
        }
        Career::Outcome oc;
        if (th && counts) {
            oc = career.recordMission(p, *th, r, out.flightSeconds, remaining);
        } else {
            oc.score = scoreMission(r);
            oc.score.counts = counts && th != nullptr;
        }
        if (!isTrainee()) career.save();
        else if (p.status != PilotStatus::Ready) p.status = PilotStatus::Ready;   // the trainee can't be killed for good

        missionReport(p, theaterName, r, oc, counts, rq.training);
        if (closed()) return;
        if (oc.recorded) {
            if (oc.promoted) {
                header();
                messageBox(t, "Promotion",
                           {std::string("Congratulations ") + p.name + ",", "for your fine display of combat prowess, you have been", std::string("promoted to the rank of ") + rankName(oc.newRank, false) + "."});
            }
            for (int m : oc.newMedals) {
                if (closed()) return;
                header();
                messageBox(t, medalName(m), {p.name + " awarded the", medalName(m), std::string("for ") + medalCitation(m) + "."});
            }
            header();
            if (p.status == PilotStatus::Killed) messageBox(t, "Killed in Action", {p.name + " has been killed in action.", "", "The pilot can be brought back with the", "\"resurrect pilot\" selection of the duty roster."});
            if (p.status == PilotStatus::Stockade) messageBox(t, "Captured", {p.name + " has been captured by the enemy.", fmt("Release is scheduled for %s.", p.releaseDate.c_str())});
        }
    }

    void missionReport(const Pilot& p, const std::string& theaterName, const ResultBlock& r, const Career::Outcome& oc, bool counts, bool training) {
        header();
        t.box(2, 3, 76, 20, WINDOW, training ? "MISSION REPORT" : "MISSION REPORT:  " + theaterName + " Theater of Operations");
        const std::string who = std::string(!isTrainee() ? rankName(p.rank, false) : "Trainee") + " " + p.name;
        t.put(4, 4, "PILOT:  " + who, WINDOW);
        std::string status;
        for (const std::string& w : statusWords(r.status)) status += w + "  ";
        if (status.empty()) status = "MISSION-ABORTED";
        t.put(4, 5, "STATUS: " + status, WINDOW);
        const Tally k = oc.score.kills;
        auto row = [&](int y, const char* label, int value) {
            t.put(6, y, padRight(label, 26), WINDOW);
            t.put(34, y, padLeft(fmt("%d", value), 6), WINDOW);
        };
        t.put(34, 7, " This", WINDOW_TITLE);
        t.put(44, 7, "Career", WINDOW_TITLE);
        t.put(56, 7, "Best Mission", WINDOW_TITLE);
        t.put(4, 8, "PRIMARY KILLS:", WINDOW_TITLE);
        row(9, "Mission Objective", k.objectives);
        row(10, "TRFRU/Fuel Dumps", k.fuelDumps);
        row(11, "AAA Batteries", k.aaa);
        row(12, "Other Sites", k.sites);
        t.put(4, 14, "SECONDARY KILLS:", WINDOW_TITLE);
        row(15, "Ground Transports", k.vehicles);
        row(16, "Enemy Missiles", k.missiles);
        row(17, "Number of Planes Lost", r.planesLost);
        row(18, "Friendly Sites Lost", r.friendliesLost);
        row(19, "Total Damage", r.maxDamage);
        if (!isTrainee() && oc.recorded) {   // the career columns
            const Stats& c = p.career;
            const int cv[4] = {c.kills.objectives, c.kills.fuelDumps, c.kills.aaa, c.kills.sites};
            const int bv[4] = {c.bestKills.objectives, c.bestKills.fuelDumps, c.bestKills.aaa, c.bestKills.sites};
            for (int i = 0; i < 4; i++) {
                t.put(46, 9 + i, padLeft(fmt("%d", cv[i]), 6), WINDOW);
                t.put(60, 9 + i, padLeft(fmt("%d", bv[i]), 6), WINDOW);
            }
            t.put(46, 15, padLeft(fmt("%d", c.kills.vehicles), 6), WINDOW);
            t.put(46, 16, padLeft(fmt("%d", c.kills.missiles), 6), WINDOW);
            t.put(46, 17, padLeft(fmt("%d", c.planesLost), 6), WINDOW);
            t.put(46, 18, padLeft(fmt("%d", c.friendliesLost), 6), WINDOW);
            t.put(60, 15, padLeft(fmt("%d", c.bestKills.vehicles), 6), WINDOW);
            t.put(60, 16, padLeft(fmt("%d", c.bestKills.missiles), 6), WINDOW);
        }
        if (oc.recorded || oc.score.counts) {
            t.put(4, 21, fmt("SCORE:  %ld x %d x %d x %.2g = %ld", oc.score.raw, oc.score.home, oc.score.landing, oc.score.worst, oc.score.total), WINDOW_TITLE);
            if (!isTrainee() && oc.recorded) t.put(4, 22, fmt("TOTAL SCORE:  %ld", p.career.points), WINDOW_TITLE);
        } else {
            t.put(4, 21, "This mission is not recorded: " + std::string(r.status & ST_ABORT_CTRL_C ? "the mission was aborted." : "an invulnerable plane was flown."), WINDOW_TITLE);
        }
        t.put(4, 24 - 2, "Type almost any key to continue...", WINDOW);
        t.waitKey();
    }

    // ---- combat missions ----
    enum { C_FLY = 1, C_OPEN, C_ACTIVATE, C_DELETE, C_VIEW, C_PLANE, C_MODEL, C_COMMENTS, C_INFO, C_RETURN };
    void combatMissions() {
        while (!closed()) {
            const std::vector<MenuItem> items = {
                {"\\Fly mission", C_FLY},
                {"\\Open a new theater of operations", C_OPEN},
                {"\\Activate an open theater", C_ACTIVATE},
                {"\\Delete a theater", C_DELETE},
                {"\\View status of a theater", C_VIEW},
                {"toggle invulnerable \\Plane", C_PLANE},
                {"toggle aerodynamic \\Model", C_MODEL},
                {"toggle inflight \\Comments", C_COMMENTS},
                {"get \\Information about this menu", C_INFO},
                {"\\Return to previous menu", C_RETURN},
            };
            const MenuResult r = runMenu(t, 4, 12, 40, items, combatSel, [&] { drawCombat(); });
            if (r.closed || r.id == -1 || r.id == C_RETURN) return;
            switch (r.id) {
            case C_FLY: flyCombat(); break;
            case C_OPEN: openTheater(); break;
            case C_ACTIVATE: activateTheater(); break;
            case C_DELETE: deleteTheater(); break;
            case C_VIEW: viewTheater(); break;
            case C_PLANE: s.invulnerableCombat = !s.invulnerableCombat; break;
            case C_MODEL: s.beginnerModel = !s.beginnerModel; break;
            case C_COMMENTS: s.comments = !s.comments; break;
            case C_INFO: info(Info::Combat, "Combat Missions Information"); break;
            }
            saveSettings(s);
            if (toMain) return;
        }
    }

    void drawCombat() {
        header();
        Pilot& p = pilot();
        t.box(3, 4, 44, 19, WINDOW, "Theater of Operations Menu");
        t.put(5, 5, "Current Pilot: " + p.name, WINDOW);
        std::string cur = "none";
        if (p.current >= 0 && p.current < static_cast<int>(p.theaters.size())) cur = theaterTitle(p.theaters[p.current]);
        t.put(5, 6, "Active Theater: " + cur, WINDOW);
        t.put(5, 7, std::string(s.invulnerableCombat ? "(Inv) " : "(Std) ") + (s.beginnerModel ? "(Beginner) " : "(Advanced) ") + (s.comments ? "" : "(No comments)"), WINDOW);
        t.put(5, 9, "Choose with arrow keys or letter", WINDOW);
        t.box(49, 4, 29, 7, WINDOW, "Theaters-of-Operation:");
        for (size_t i = 0; i < theaterInfos.size() && i < 5; i++) t.put(50, 5 + static_cast<int>(i), padRight(theaterInfos[i].name, 27), WINDOW);
        t.box(49, 11, 29, 12, WINDOW, "Your Open Theaters:");
        for (size_t i = 0; i < p.theaters.size() && i < 10; i++)
            t.put(50, 12 + static_cast<int>(i), padRight(std::string(static_cast<int>(i) == p.current ? "> " : "  ") + theaterTitle(p.theaters[i]), 27), WINDOW);
    }

    int openedCount() { return static_cast<int>(pilot().theaters.size()); }

    void openTheater() {
        Pilot& p = pilot();
        if (openedCount() >= MAX_THEATERS) {
            messageBox(t, "Too Many Theaters", {"Sorry, " + p.name + " already has", "the maximum number of available theaters open.", "", "You will have to delete one first."});
            return;
        }
        if (theaterInfos.empty()) {
            messageBox(t, "No Theaters", {"Sorry, there are no theaters to choose from.", "", "The Theaters of Action should be files", "stored in the subdirectory `def'"});
            return;
        }
        std::vector<std::string> entries;
        for (const TheaterInfo& i : theaterInfos) entries.push_back(i.name);
        header();
        const int pick = chooseFromList("Choose a Theater of Operations", "Select with arrow keys, <CR> to choose, <ESC> to abort", entries);
        if (pick < 0) return;
        const std::vector<uint8_t> cct = readBytes(defDir + theaterNames[pick] + ".CCT");
        UniverseSummary sum;
        if (!summarizeUniverse(cct, sum)) {
            messageBox(t, "Cannot open theater", {"Cannot read the theater file."});
            return;
        }
        const int idx = career.openTheater(p, theaterNames[pick], sum.objectivesRemaining, std::max(sum.planes, 1));
        if (idx < 0) return;
        writeBytes(career.theaterFile(p.theaters[idx]), cct);
        p.theaters[idx].objectivesTotal = sum.objectivesRemaining;
        p.theaters[idx].storagePlanes = std::max(sum.brownPlanes, 3);
        if (!isTrainee()) career.save();
        const TheaterInfo& ti = theaterInfos[pick];
        std::vector<std::string> lines = ti.description;
        if (lines.empty()) lines.push_back("There is no general information available about this Theater of Operations.");
        lines.push_back("");
        lines.push_back(fmt("%d mission objective%s.", sum.objectivesRemaining, plural(sum.objectivesRemaining).c_str()));
        messageBox(t, ti.name, lines);
    }

    int pickOpenTheater(const std::string& title, const std::string& prompt, const std::string& noneTitle, const std::string& noneText) {
        Pilot& p = pilot();
        if (p.theaters.empty()) {
            messageBox(t, noneTitle, {p.name + " has no", noneText, "", "You will have to open one first."});
            return -1;
        }
        std::vector<std::string> entries;
        for (size_t i = 0; i < p.theaters.size(); i++) entries.push_back(std::string(static_cast<int>(i) == p.current ? "* " : "  ") + theaterTitle(p.theaters[i]));
        header();
        return chooseFromList(title, prompt, entries, std::max(0, p.current));
    }

    void activateTheater() {
        const int i = pickOpenTheater("Activate an Open Theater", "Select theater with arrow keys, <CR> to activate", "No Theaters to Activate", "open theaters to activate.");
        if (i >= 0) {
            pilot().current = i;
            if (!isTrainee()) career.save();
        }
    }

    void deleteTheater() {
        const int i = pickOpenTheater("Delete a Theater", "Select theater with arrow keys, <CR> to Delete", "No Theaters to Delete", "open theaters to delete.");
        if (i < 0) return;
        Pilot& p = pilot();
        if (p.theaters[i].complete) messageBox(t, "Theater is Complete", {"Warning!  theater " + theaterTitle(p.theaters[i]), "has already been completed!  Deleted theaters", "can not be counted toward Combat Master status."});
        if (confirmBox(t, "Delete a Theater", {"Are you sure you want to delete this theater?", "", "Type `y' to confirm, anything else cancels"}, 'y', false)) {
            career.deleteTheater(p, i);
            if (!isTrainee()) career.save();
        }
    }

    void viewTheater() {
        const int i = pickOpenTheater("View Theater Records", "Select theater with arrow keys, <CR> to View", "No Theaters to View", "open theaters to view.");
        if (i < 0) return;
        const Pilot& p = pilot();
        const TheaterRecord& th = p.theaters[i];
        header();
        t.box(8, 5, 64, 15, WINDOW, theaterTitle(th));
        t.put(10, 6, fmt("%d mission%s.  %d objective%s killed, %d remain.", th.stats.missions, plural(th.stats.missions).c_str(),
                         th.stats.kills.objectives, plural(th.stats.kills.objectives).c_str(), th.objectivesRemaining), WINDOW);
        t.put(10, 7, th.planesLeft > 0 ? fmt("There are %d plane%s in the theater.", th.planesLeft, plural(th.planesLeft).c_str()) : "There are no planes left in the theater.", WINDOW);
        t.put(10, 8, th.storagePlanes > 0 ? fmt("There are %d plane%s remaining in storage.", th.storagePlanes, plural(th.storagePlanes).c_str()) : "There are no planes remaining in storage.", WINDOW);
        t.put(10, 10, fmt("Total points = %ld   Avg = %ld", th.stats.points, th.stats.missions ? th.stats.points / th.stats.missions : 0), WINDOW);
        t.put(10, 11, fmt("Primary Kills = %d   Avg = %d", th.stats.kills.primary(), th.stats.missions ? th.stats.kills.primary() / th.stats.missions : 0), WINDOW);
        t.put(10, 12, fmt("Other Kills = %d", th.stats.kills.other()), WINDOW);
        t.put(10, 14, "Best mission score = " + fmt("%ld:", th.stats.best), WINDOW);
        t.put(10, 15, fmt(" %d primary kills,  %d other kills", th.stats.bestKills.primary(), th.stats.bestKills.other()), WINDOW);
        if (th.complete) t.put(10, 17, "This theater is complete.", WINDOW_TITLE);
        t.put(10, 18, "Type almost any key to continue...", WINDOW);
        t.waitKey();
    }

    void flyCombat() {
        Pilot& p = pilot();
        if (p.current < 0 || p.current >= static_cast<int>(p.theaters.size())) {
            messageBox(t, "No Current Theater", {"Sorry, there is no active theater-of-operation.", "", "You may choose a theater with the \"Open\" menu", "selection by typing `O'.",
                                                   "You may activate one of your theaters with the \"Activate\" menu", "selection by typing `A'."});
            return;
        }
        if (isTrainee()) {
            const bool go = messageBoxYesNo("Trainee Pilot", {" You may fly combat missions as a trainee,", " but your progress will be deleted when", " you leave the program.  It is recommended",
                                                              " that you create a pilot by using the duty", " roster selection from the main menu.", "", "Type <CR> to continue, <ESC> to abort"});
            if (!go) return;
        }
        TheaterRecord& th = p.theaters[p.current];
        const TheaterInfo* ti = nullptr;
        for (const TheaterInfo& i : theaterInfos)
            if (i.file == th.def) ti = &i;
        // the files: the pilot's copy of the universe (the history) and the text of the theater
        const std::string cct = career.theaterFile(th);
        if (!std::filesystem::exists(cct)) {   // a missing copy starts afresh
            writeBytes(cct, readBytes(defDir + th.def + ".CCT"));
        }
        if (th.objectivesRemaining <= 0) {
            header();
            centeredBox(t, "Theater is Complete", {"There are no mission objectives remaining in", "this Theater of Operations.", "", "Type <ESC> to abort,", "or almost anything else to continue..."});
            const UiKey k = t.waitKey();
            if (k.quit || k.code == SDLK_ESCAPE) return;
        }
        // the airfield: shareware theaters have one
        header();
        std::vector<std::string> lines;
        lines.push_back(fmt("%s", ti && !ti->airfield.empty() ? ti->airfield.c_str() : "Your airfield"));
        lines.push_back(fmt("(%d plane%s  %d MO)", th.planesLeft, plural(th.planesLeft).c_str(), th.objectivesRemaining));
        lines.push_back("");
        lines.push_back("There is only one airfield in this Theater-of-Operation.");
        lines.push_back("");
        lines.push_back("Type <CR> to fly, `i' for detailed info, or <ESC> to abort mission.");
        bool stock = false;
        for (;;) {
            centeredBox(t, "Your Starting Airfield", lines);
            const UiKey k = t.waitKey();
            if (k.quit) return;
            if (k.code == SDLK_ESCAPE) return;
            if (k.ch == 'i' || k.ch == 'I') {
                std::vector<std::string> all = ti ? ti->airfieldBanner : std::vector<std::string>();
                if (ti)
                    for (const MissionInfo& m : ti->missions) {
                        all.push_back("");
                        all.push_back(" Mission Code Name : " + m.codeName);
                        all.push_back(" Direction         : " + m.direction + "     Distance: " + m.distance + "     Difficulty: " + m.difficulty);
                        for (const std::string& b : m.briefing) all.push_back(b);
                    }
                if (all.empty()) all.push_back("There is no information about this airfield.");
                pager(t, "Detailed Info", all, "Type PGUP, PGDN, <CR> to return...");
                continue;
            }
            if (k.code == SDLK_RETURN || k.code == SDLK_KP_ENTER) break;
        }
        if (th.planesLeft <= 0) {   // no regular plane: one comes out of storage
            if (th.storagePlanes > 0) {
                header();
                if (messageBoxYesNo("No Planes at this Airfield", {"Warning!  There are no planes available at this airstrip.", fmt("        (However, you have %d plane%s in storage.)", th.storagePlanes, plural(th.storagePlanes).c_str()),
                                                                   "        Type <CR> to pull a new plane from storage.", "                    Type <ESC> to abort."})) {
                    stock = true;
                    th.storagePlanes--;
                } else {
                    return;
                }
            } else {
                messageBox(t, "No Planes", {"There are no planes left, in the theater or in storage.", "", "Open a fresh theater to go on."});
                return;
            }
        }
        // the briefing
        header();
        const std::string who = std::string(isTrainee() ? "Trainee" : rankName(p.rank, true)) + " " + p.name;
        std::vector<std::string> brief = {who + ", prepare for your mission!", "", "Your Theater of Operation:  " + (ti ? ti->name : th.def), ""};
        if (ti) brief.insert(brief.end(), ti->description.begin(), ti->description.end());
        brief.push_back("");
        if (s.invulnerableCombat) brief.push_back("This mission will not be recorded because an invulnerable plane is flown.");
        brief.push_back("Type <CR> to continue, <ESC> to abort");
        centeredBox(t, "Mission Briefing", brief);
        {
            const UiKey k = t.waitKey();
            if (k.quit || k.code == SDLK_ESCAPE) return;
        }
        MissionRequest rq;
        rq.training = false;
        rq.cctPath = cct;
        rq.twrPath = defDir + th.def + ".TWR";
        rq.invincible = s.invulnerableCombat;
        rq.beginnerModel = s.beginnerModel;
        rq.comments = s.comments;
        rq.stockPlane = stock;
        rq.hd = s.hd;
        rq.smooth = s.smooth;
        rq.fps = s.fps;
        rq.modernPanel = s.modernPanel;
        rq.useGamepad = s.joystick;
        rq.invertPitch = s.invertPitch;
        fly(rq, &th, ti ? ti->name : th.def);
        if (!isTrainee()) career.save();
    }

    bool messageBoxYesNo(const std::string& title, const std::vector<std::string>& lines) {
        centeredBox(t, title, lines);
        const UiKey k = t.waitKey();
        return !k.quit && (k.code == SDLK_RETURN || k.code == SDLK_KP_ENTER);
    }

    // ---- the duty roster ----
    enum { D_CREATE = 1, D_ACTIVATE, D_DEACTIVATE, D_DELETE, D_RESURRECT, D_RENAME, D_VIEW, D_INFO, D_RETURN };

    std::vector<std::string> pilotLines() {
        std::vector<std::string> out;
        for (size_t i = 0; i < career.pilots().size(); i++) {
            const Pilot& p = career.pilots()[i];
            std::string tag = static_cast<int>(i) == career.activeIndex() ? "(active)" : p.status == PilotStatus::Killed ? "(K.I.A.)"
                              : p.status == PilotStatus::Captured ? "(captured)" : p.status == PilotStatus::Stockade ? "(stockade)" : "(ready for action)";
            out.push_back(padRight(p.name, 19) + tag);
        }
        return out;
    }

    void drawDuty() {
        header();
        t.box(3, 4, 40, 17, WINDOW, "Duty Roster");
        t.put(5, 5, "Use arrow keys or type highlighted", WINDOW);
        t.put(5, 6, "    letter to choose action:", WINDOW);
        t.put(5, 17, "Active pilot:  " + (career.active() ? career.active()->name : std::string("trainee")), WINDOW);
        t.box(44, 4, 34, 17, WINDOW, "Pilots:");
        const std::vector<std::string> lines = pilotLines();
        for (size_t i = 0; i < lines.size() && i < 15; i++) t.put(45, 5 + static_cast<int>(i), padRight(lines[i], 32), WINDOW);
        if (lines.empty()) t.put(46, 6, "No pilots yet.", WINDOW);
    }

    void dutyRoster() {
        while (!closed()) {
            const std::vector<MenuItem> items = {
                {"\\Create Pilot", D_CREATE},
                {"\\Activate Pilot", D_ACTIVATE},
                {"\\Deactivate Pilot", D_DEACTIVATE},
                {"de\\Lete Pilot", D_DELETE},
                {"r\\Esurrect Pilot", D_RESURRECT},
                {"re\\Name Pilot", D_RENAME},
                {"\\View Pilot Records", D_VIEW},
                {"get \\Information about this menu", D_INFO},
                {"\\Return to previous menu", D_RETURN},
            };
            const MenuResult r = runMenu(t, 4, 8, 38, items, dutySel, [&] { drawDuty(); });
            if (r.closed || r.id == -1 || r.id == D_RETURN) return;
            switch (r.id) {
            case D_CREATE: createPilot(); break;
            case D_ACTIVATE: activatePilot(); break;
            case D_DEACTIVATE:
                if (!career.active()) messageBox(t, "No Pilot to deactivate", {"There is no pilot in the list to be deactivated.", "", "You will have to activate one first."});
                else career.deactivate();
                break;
            case D_DELETE: deletePilot(); break;
            case D_RESURRECT: resurrectPilot(); break;
            case D_RENAME: renamePilot(); break;
            case D_VIEW: viewPilots(); break;
            case D_INFO: info(Info::DutyRoster, "Duty Roster Menu Information"); break;
            }
            career.save();
            saveSettings(s);
        }
    }

    // a pilot from the list; filter says which ones can be chosen
    int pickPilot(const std::string& title, const std::string& prompt, const std::string& noneTitle, const std::vector<std::string>& noneText,
                  const std::function<bool(const Pilot&)>& filter) {
        std::vector<std::string> entries;
        std::vector<int> index;
        for (size_t i = 0; i < career.pilots().size(); i++)
            if (filter(career.pilots()[i])) {
                const Pilot& p = career.pilots()[i];
                entries.push_back(padRight(std::string(rankName(p.rank, true)) + " " + p.name, 30));
                index.push_back(static_cast<int>(i));
            }
        if (entries.empty()) {
            messageBox(t, noneTitle, noneText);
            return -1;
        }
        drawDuty();
        const int pick = chooseFromList(title, prompt, entries);
        return pick < 0 ? -1 : index[pick];
    }

    void createPilot() {
        if (static_cast<int>(career.pilots().size()) >= MAX_PILOTS) {
            messageBox(t, "Too many pilots", {"A new pilot cannot be created because the pilot list", " is full.", "", "You will have to delete one first."});
            return;
        }
        std::string name;
        drawDuty();
        if (!inputLine(t, "Create New Pilot", {"Enter name for new pilot.", fmt("(Maximum of %d characters)", static_cast<int>(MAX_NAME))}, name, MAX_NAME, "") || name.empty()) return;
        // no '|' in names (they are a separator in the file)
        name.erase(std::remove(name.begin(), name.end(), '|'), name.end());
        if (career.findPilot(name) >= 0) {
            messageBox(t, "Pilot exists", {"Sorry, Pilot " + name + " already exists."});
            return;
        }
        const int idx = career.createPilot(name);
        if (idx < 0) return;
        if (confirmBox(t, "Activate " + name + "?", {"Activate " + name + "? [Yes or No]", "", "Type <CR> for yes"}, 'y', true)) career.activate(idx);
    }

    void activatePilot() {
        const int i = pickPilot("Choose Active Pilot", "Select pilot with arrow keys, <CR> to activate", "No Pilot to activate",
                                {"There are no pilots in the list to be activated.", "", "You will have to create one first."}, [](const Pilot&) { return true; });
        if (i < 0) return;
        const Pilot& p = career.pilots()[i];
        if (p.dead()) {
            const char* what = p.status == PilotStatus::Killed ? "killed in action." : p.status == PilotStatus::Captured ? "captured by the enemy." : "confined to the stockade.";
            std::vector<std::string> lines = {"Pilot Cannot be Activated", p.name + " has been " + what};
            if (p.status == PilotStatus::Stockade) lines.push_back(fmt("Release is scheduled for %s.", p.releaseDate.c_str()));
            lines.push_back("");
            lines.push_back("Do you want to resurrect the pilot?  (Y or N)");
            if (confirmBox(t, "Pilot Cannot be Activated", lines, 'y', false)) {
                career.resurrect(i);
                career.activate(i);
            }
            return;
        }
        career.activate(i);
    }

    void deletePilot() {
        const int i = pickPilot("Choose Pilot to Delete", "Select pilot with arrow keys, <CR> to delete", "No Pilots to Delete",
                                {"There are no pilots in the list to delete."}, [](const Pilot&) { return true; });
        if (i < 0) return;
        if (confirmBox(t, "Confirm Deletion", {"Type <ENTER> to delete " + career.pilots()[i].name, " Almost anything else cancels..."}, '\0', true)) career.deletePilot(i);
    }

    void resurrectPilot() {
        const int i = pickPilot("Choose Pilot to Resurrect", "Select pilot with arrow keys, <CR> to resurrect", "No Pilot to Resurrect",
                                {"There are no dead or captured pilots in the list.", "", "You will have to kill or lose one first."},
                                [](const Pilot& p) { return p.dead(); });
        if (i < 0) return;
        if (confirmBox(t, "Confirm Resurrection", {"Type <ENTER> to resurrect " + career.pilots()[i].name, " Anything else cancels..."}, '\0', true)) career.resurrect(i);
    }

    void renamePilot() {
        const int i = pickPilot("Choose Pilot to Rename", "Select pilot with arrow keys, <CR> to rename", "No Pilot to Rename",
                                {"There is no pilot in the list to rename."}, [](const Pilot&) { return true; });
        if (i < 0) return;
        std::string name = career.pilots()[i].name;
        drawDuty();
        if (inputLine(t, "Rename Pilot", {"Enter new name for " + career.pilots()[i].name, fmt("(Maximum of %d characters)", static_cast<int>(MAX_NAME))}, name, MAX_NAME, "") && !name.empty()) {
            name.erase(std::remove(name.begin(), name.end(), '|'), name.end());
            if (!career.rename(i, name)) messageBox(t, "Pilot exists", {"Sorry, Pilot " + name + " already exists."});
        }
    }

    std::vector<std::string> pilotRecord(const Pilot& p, bool active) {
        std::vector<std::string> l;
        l.push_back(fmt("         PILOT:  %s %s%s", rankName(p.rank, false), p.name.c_str(),
                        p.resurrections ? fmt(", %d resurrection%s", p.resurrections, plural(p.resurrections).c_str()).c_str() : ""));
        l.push_back(fmt("                 %d mission%s", p.career.missions, plural(p.career.missions).c_str()));
        l.push_back("                 Flying time = " + flightTime(p.career.flightSeconds));
        std::string st = p.status == PilotStatus::Killed ? "K.I.A." : p.status == PilotStatus::Captured ? "CAPTURED"
                         : p.status == PilotStatus::Stockade ? "CONFINED TO STOCKADE until " + p.releaseDate : active ? "ACTIVE" : "WAITING FOR DUTY";
        l.push_back("        STATUS:  " + st);
        l.push_back(fmt("    HIGH SCORE:  %-9ld      TOTAL SCORE:  %ld", p.career.best, p.career.points));
        l.push_back("");
        l.push_back("                                    Career       Best Mission");
        l.push_back(" PRIMARY KILLS:");
        auto row = [&](const char* label, int career, int best) { l.push_back(fmt("   %-30s%6d        %6d", label, career, best)); };
        row("Mission Objective", p.career.kills.objectives, p.career.bestKills.objectives);
        row("TRFRU/Fuel Dumps", p.career.kills.fuelDumps, p.career.bestKills.fuelDumps);
        row("AAA Batteries", p.career.kills.aaa, p.career.bestKills.aaa);
        row("Other Sites", p.career.kills.sites, p.career.bestKills.sites);
        l.push_back(" SECONDARY KILLS:");
        row("Ground Transports", p.career.kills.vehicles, p.career.bestKills.vehicles);
        row("Enemy Missiles", p.career.kills.missiles, p.career.bestKills.missiles);
        l.push_back(fmt("   %-30s%6d", "Number of Planes Lost", p.career.planesLost));
        l.push_back(fmt("   %-30s%6d", "Friendly Sites Lost", p.career.friendliesLost));
        l.push_back(fmt("   %-30s%6ld", "Total Damage", p.career.damage));
        l.push_back("");
        l.push_back(" DECORATIONS:");
        bool any = false;
        for (int m = 0; m < NMEDALS; m++)
            if (p.medals & (1u << m)) {
                l.push_back(fmt("   %s  (%s)", medalName(m), medalCitation(m)));
                any = true;
            }
        if (!any) l.push_back("   none");
        return l;
    }

    void viewPilots() {
        if (career.pilots().empty()) {
            messageBox(t, "No Pilots to View", {"There are no pilots in the list to be viewed", "", "You will have to create one first."});
            return;
        }
        int i = pickPilot("View Pilot Records", "Select pilot with arrow keys, <CR> to view", "No pilots to view.", {"No pilots to view."}, [](const Pilot&) { return true; });
        while (i >= 0 && !closed()) {
            const Pilot& p = career.pilots()[i];
            header();
            t.box(1, 3, 78, 21, WINDOW, "Pilot Records");
            const std::vector<std::string> lines = pilotRecord(p, i == career.activeIndex());
            for (size_t k = 0; k < lines.size() && k < 19; k++) t.put(3, 4 + static_cast<int>(k), lines[k], WINDOW);
            t.put(3, 23, "Type UP/DOWN for next/prev, or ESC, BS, or CR to exit", WINDOW_TITLE);
            const UiKey k = t.waitKey();
            if (k.quit || k.code == SDLK_ESCAPE || k.code == SDLK_BACKSPACE || k.code == SDLK_RETURN) return;
            const int n = static_cast<int>(career.pilots().size());
            if (k.code == SDLK_DOWN || k.code == SDLK_RIGHT) i = (i + 1) % n;
            else if (k.code == SDLK_UP || k.code == SDLK_LEFT) i = (i + n - 1) % n;
        }
    }

    // ---- the high score lists ----
    enum { H_SCORES = 1, H_CAREERS, H_FAST, H_DELETE, H_INFO, H_RETURN };

    void highScores() {
        while (!closed()) {
            const std::vector<MenuItem> items = {
                {"see list of high \\Scores", H_SCORES},
                {"see list of top \\Careers", H_CAREERS},
                {"see list of \\Fastest careers", H_FAST},
                {"\\Delete one of the above lists", H_DELETE},
                {"get \\Information about this menu", H_INFO},
                {"\\Return to previous menu", H_RETURN},
            };
            const MenuResult r = runMenu(t, 24, 9, 34, items, hiSel, [&] {
                header();
                t.box(22, 6, 38, 11, WINDOW, "High Score Menu");
                t.put(24, 7, "Use arrow keys or type highlighted", WINDOW);
            });
            if (r.closed || r.id == -1 || r.id == H_RETURN) return;
            switch (r.id) {
            case H_SCORES: showList(ScoreList::HighScores); break;
            case H_CAREERS: showList(ScoreList::TopCareers); break;
            case H_FAST: showList(ScoreList::FastCareers); break;
            case H_DELETE: deleteList(); break;
            case H_INFO: info(Info::HighScores, "Information about High Score Menu"); break;
            }
        }
    }

    void showList(ScoreList which) {
        const std::vector<ScoreEntry>& list = career.list(which);
        std::vector<std::string> lines;
        const char* title = which == ScoreList::HighScores ? "High Scores" : which == ScoreList::TopCareers ? "Top Careers" : "Fastest Careers";
        if (which == ScoreList::HighScores) lines.push_back("Name                       Score  Theater-of-Operations      Kills");
        else if (which == ScoreList::TopCareers) lines.push_back("Name                 Total Score  Missions Resurrections     Kills");
        else lines.push_back("Name                 Total Score  Missions Resurrections   Points/Misn");
        lines.push_back("");
        for (const ScoreEntry& e : list) {
            const std::string name = (e.deleted ? "*" : " ") + e.name;
            if (which == ScoreList::HighScores)
                lines.push_back(fmt("%-24s %9ld  %-20s  %3d %3d %3d", name.c_str(), e.score, e.theater.c_str(), e.objectives, e.primary, e.other));
            else if (which == ScoreList::TopCareers)
                lines.push_back(fmt("%-20s %9ld  %-8d %-8d    %4d %4d %4d", name.c_str(), e.score, e.missions, e.resurrections, e.objectives, e.primary, e.other));
            else
                lines.push_back(fmt("%-20s %9ld  %-8d %-8d   %16ld", name.c_str(), e.score, e.missions, e.resurrections, e.average));
        }
        if (list.empty()) lines.push_back("(There is nothing in this list yet.)");
        header();
        pager(t, title, lines, "Type <CR> or <ESC> to return...");
    }

    void deleteList() {
        const std::vector<MenuItem> items = {{"delete the list of top \\Scores", 1}, {"delete the list of top \\Careers", 2},
                                             {"delete the list of \\Fastest careers", 3}, {"\\Return to previous menu", 4}};
        int sel = 0;
        const MenuResult r = runMenu(t, 22, 9, 40, items, sel, [&] {
            header();
            t.box(20, 6, 44, 8, WINDOW, "Delete a Score List");
        });
        if (r.closed || r.id == -1 || r.id == 4) return;
        const ScoreList which = r.id == 1 ? ScoreList::HighScores : r.id == 2 ? ScoreList::TopCareers : ScoreList::FastCareers;
        const char* name = r.id == 1 ? "high score" : r.id == 2 ? "top career" : "fast career";
        if (confirmBox(t, "Confirm Deletion", {fmt("Type <ENTER> to delete %s list", name), " Almost anything else cancels..."}, '\0', true)) {
            career.clearList(which);
            career.save();
        }
    }
};

bool runControlSetup(Window& window, Settings& settings, bool inMission) {
    TextScreen screen(window);
    SetupMenus menus{screen, settings};
    menus.inMission = inMission;
    menus.applyBindings();
    return menus.controlSetup();
}

FrontEnd::FrontEnd(TextScreen& screen, Settings& settings, MusicBox* music, MissionLauncher launch)
    : m_impl(std::make_unique<Impl>(screen, settings, music, std::move(launch))) {}

FrontEnd::~FrontEnd() = default;

void FrontEnd::run() {
    m_impl->applyBindings();
    if (m_impl->music && m_impl->s.music) m_impl->music->playMenu();
    m_impl->mainMenu();
    saveSettings(m_impl->s);
    m_impl->career.save();
}

} // namespace corncob
