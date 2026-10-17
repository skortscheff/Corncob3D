// TowerScreen.hpp - The screens the game shows from inside a flight (the original's TOWER.EXE)
//
// F2 (or stepping into the tower on foot) shows the information of the airfield: its missions, with where
// each starts; the arrow keys move between them and <CR> sends the plane there. F1 is the boss screen: a
// blank DOS prompt (BOSS.TXT, which the player can edit). F3 and F6 were not in the shareware version and
// say so.

#pragma once
#include "game/TheaterFiles.hpp"
#include "platform/Window.hpp"
#include <cstdint>
#include <string>

namespace corncob {

struct TowerChoice {
    bool mission = false;       // a mission was chosen
    int32_t x = 0, y = 0, z = 0, yaw = 0;
};

// The airfield's information with its missions. `canGo`: the plane is flying (a mission can be chosen).
TowerChoice runTowerInfo(Window& window, const TheaterInfo& theater, bool canGo);
// The boss screen: the text of BOSS.TXT until a key is pressed
void runBossScreen(Window& window);
// A message in the middle of a black screen, for a key
void runTowerMessage(Window& window, const std::string& text);

} // namespace corncob
