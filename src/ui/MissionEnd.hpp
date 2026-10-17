// MissionEnd.hpp - The pictures shown when a mission ends (PICT.ASM pict)
//
// PICT.DAT holds the pictures (see loadMissionScript); which ones are shown comes from the mission status
// bits the main loop leaves behind:
//   bit 14 abort by ctrl-c       bit 13 invincible plane      bit 11 killed by an object
//   bit 10 lost in space         bit 9 crash-landed at home   bit 8 rescued      bit 7 hit a building
//   bit 6 crash-landed, not home bit 5 ejected                bit 4 home         bit 3 captured
//   bit 2 landed at home         bit 1 killed                 bit 0 aborted in the air
// The pictures, by number: 0 happy landing, 1 rescue, 2 crash-landed away from home, 3 crash-landed at
// home, 4 flyby, 5 flyby home, 6 the brig (friendlies destroyed), 7 dead fall, 8 captured, 9 unknown, not
// in the plane, 10-13 the corpse (a shell, a death ball, a barrage balloon or crater ...), 14 lost in space.

#pragma once
#include "../platform/Window.hpp"
#include "Intro.hpp"
#include <cstdint>
#include <vector>

namespace corncob {

// The numbers of the pictures PICT.ASM shows, in order, for these end-of-mission values. `nfdest` is the
// number of friendlies destroyed, `deathcode` the type of the object that killed the player.
std::vector<int> missionPictures(uint16_t missionStatus, uint16_t nfdest, uint16_t deathcode);

// Shows the pictures one after the other (a key moves on). When shotPrefix is set it runs unattended and
// saves one BMP per picture as <shotPrefix>NN.bmp.
IntroResult runMissionEnd(Window& window, const std::vector<int>& pictures, const char* shotPrefix = nullptr);

} // namespace corncob
