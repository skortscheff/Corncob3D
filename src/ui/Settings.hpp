// Settings.hpp - The choices the menus keep between runs (the original's moag.cfg and 3d.cfg)
//
// A plain text file of "name value" lines in the save directory. The save directory is the platform's
// per-user application data folder, or the folder given on the command line (--save-dir) or in the
// CORNCOB_SAVE environment variable.

#pragma once
#include <string>
#include <utility>
#include <vector>

namespace corncob {

struct TrainingSetup {
    // The training mission of the main menu: the numbers of enemy sites and how wicked they are (1-8
    // increasing, 0 a random spread). The default is 5 missile (KLA) sites with a random spread, 6 AAA
    // batteries with a random spread and 2 vehicle sites of wickedness 4, in an invulnerable plane.
    int kla = 5, aaa = 6, vehicles = 2;
    int klaWicked = 0, aaaWicked = 0, vehWicked = 4;
    bool invulnerable = true;
    void defaults() { *this = TrainingSetup(); }
};

struct Settings {
    bool music = true;               // the title and menu music
    bool joystick = true;            // false: the keyboard only; true: a gamepad or joystick is used when there is one
    bool invertPitch = false;        // the pitch of the gamepad stick reversed (the flight stick way: pull back for nose up)
    bool manualCentering = false;    // joystick recentering: manual or automatic
    bool forceRecenter = false;
    bool comments = true;            // the inflight comments
    bool beginnerModel = false;      // the beginner aerodynamic model
    bool invulnerableCombat = false; // an invulnerable plane in combat missions (they don't count then)
    TrainingSetup training;
    std::string activePilot;         // the name of the active pilot, empty for the trainee
    int hd = 1;                      // missions drawn at 1-4 times 640x350
    bool modernPanel = true;         // the smooth instrument panel instead of the EGA one
    int fps = 60;                    // game frames per second: 60, or 20 as the original ran
    std::vector<std::pair<int, int>> bindings;   // keys the player chose: (DOS scan code, SDL scancode)
};

// the folder the saved files live in, with a trailing separator; created when it does not exist
std::string saveDirectory();
void setSaveDirectory(const std::string& dir);

bool loadSettings(Settings& s);
bool saveSettings(const Settings& s);

} // namespace corncob
