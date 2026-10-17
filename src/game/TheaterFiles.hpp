// TheaterFiles.hpp - Reading the theater files without loading the game: what a theater holds
//
// DEF/<name>.CCT is the universe (see World.hpp) and DEF/<name>.TWR the text: a parameter line, the name of the
// theater and nine lines about it ("." for an empty one), then the briefings of the airfield, in blocks:
//
//   ------- Welcome to RDU ^international Airport -------     the airfield (^ breaks the line)
//    Mission Code Name : Insult to Injury                       a mission
//    Direction         : North
//    Distance          : 8 miles
//    Difficulty        : 1
//   Sxyth 353095 -138740 167038 0                              where it starts: x y z yaw
//   ---------                                                   its briefing text
//
// The menus need: the list of theaters, how many mission objectives a universe holds and how many are
// still to do, how many planes it has, and the missions of its airfield.

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace corncob {

struct UniverseSummary {
    // The mission objectives still to do: orbs, special saucers, spies and portals that finish the mission. (The
    // scenery orbs are marked done from the start and are not counted: a fresh theater's count is all it asks.)
    int objectivesRemaining = 0;
    int planes = 0;           // regular planes in good order
    int brownPlanes = 0;      // the old planes of the storage
    int crashedPlanes = 0;
    int32_t startX = 0, startY = 0, startZ = 0;   // the start position
};

// Counts what is in a 3univ.dat (a .CCT file's bytes); false if it is not a universe
bool summarizeUniverse(const std::vector<uint8_t>& cct, UniverseSummary& out);

struct MissionInfo {
    std::string codeName;
    std::string direction, distance, difficulty;
    int32_t x = 0, y = 0, z = 0, yaw = 0;   // the start position of the mission
    std::vector<std::string> briefing;      // the text of the mission
};

struct TheaterInfo {
    std::string file;                 // MORNING
    std::string name;                 // Atkinsville (easy)
    std::vector<std::string> description;
    std::string airfield;             // "RDU international Airport"
    std::vector<std::string> airfieldBanner;   // the text before the first mission
    std::vector<MissionInfo> missions;
};

bool loadTheaterInfo(const std::string& twrPath, TheaterInfo& out);

// The theaters in a def directory (a .CCT with a .TWR), sorted by file name, without the extension
std::vector<std::string> listTheaters(const std::string& defDir);

} // namespace corncob
