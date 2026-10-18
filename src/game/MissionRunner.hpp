// MissionRunner.hpp - Flies one mission: everything 3.EXE did between the menu program's calls
//
// The original's menu program (MOAG) wrote the universe and its options into files and ran 3.EXE; when 3.EXE
// ended it read the result file. Here the front end fills in a MissionRequest, runMission sets up a fresh
// copy of the game (the data segment, the universe, the sound), flies it, and hands back what happened.

#pragma once
#include "platform/Window.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace corncob {

struct MissionRequest {
    // what is flown: a training mission builds its own universe (initperms); a combat mission reads the
    // theater files (3univ.dat / towers.dat as MOAG would have written them)
    bool training = false;
    std::string cctPath, twrPath;
    int startTower = 0;

    // the options 3.EXE got on its command line (CMDLINE.ASM)
    int kla = 0, aaa = 0, vehicles = 0;                   // -l -n -m: the numbers of sites
    int klaQuality = -1, aaaQuality = -1, vehQuality = -1;   // -t -v -w: the quality (0-7), -1 random
    bool invincible = false;                              // -i
    bool beginnerModel = false;                           // -y
    bool comments = false;                                // -{ : the inflight messages
    bool stockPlane = false;                              // -h: issue a plane from storage
    bool useGamepad = true;                               // false: the keyboard only (-k)
    bool invertPitch = false;                             // the gamepad pitch the other way

    // the display and the sound
    int hd = 1;
    bool noSound = false;
    int fps = 20;
    bool modernPanel = true;         // the smooth instrument panel over the EGA one
    bool showLoadingScreen = true;   // "Please wait one moment..." and the instrument panel fading in
    bool showEndPictures = true;     // the picture for how the mission ended

    // unattended runs (screenshots): fly shotFrames frames with the scripted keys, save the picture
    const char* shotPath = nullptr;
    int shotFrames = 8;
    const char* keyScript = nullptr;
    const char* resPath = nullptr;   // write the result block there when the mission ends

    // development aids
    int spawnHitFrame = -1;
    bool spawnEnemies = false;
};

struct MissionOutcome {
    bool over = false;           // the mission ran to its end (not abandoned by closing the window)
    bool windowClosed = false;
    uint16_t status = 0;         // the mission status bits (PICT.ASM lists them)
    uint16_t nfdest = 0;         // friendlies destroyed
    uint16_t deathcode = 0;
    long flightSeconds = 0;        // how long the mission took
    std::vector<uint8_t> result;   // the 140 byte result block (3.res)
    std::vector<uint8_t> universe; // a combat mission: the universe as 3.EXE leaves it (3univ.dat)
};

MissionOutcome runMission(Window& window, const MissionRequest& request);

} // namespace corncob
