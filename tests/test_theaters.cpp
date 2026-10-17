// test_theaters.cpp - What the menus read from the shipped theaters (assets/original, never committed)

#include "game/TheaterFiles.hpp"
#include <cstdio>
#include <fstream>
#include <iterator>

#ifndef CORNCOB_ASSET_DIR
#define CORNCOB_ASSET_DIR "assets/original"
#endif

using namespace corncob;

static int failures = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                      \
        }                                                                    \
    } while (0)

int main() {
    const std::string def = std::string(CORNCOB_ASSET_DIR) + "/DEF/";
    const std::vector<std::string> names = listTheaters(def);
    if (names.empty()) {
        std::printf("test_theaters: original files not found, skipped\n");
        return 0;
    }
    CHECK(names.size() == 4);   // DUSK MORNING WASTELND WHTSANDS
    for (const std::string& n : names) {
        std::ifstream in(def + n + ".CCT", std::ios::binary);
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        UniverseSummary s;
        CHECK(summarizeUniverse(bytes, s));
        TheaterInfo t;
        CHECK(loadTheaterInfo(def + n + ".TWR", t));
        std::printf("%s: \"%s\" objectives %d planes %d (+%d old, %d crashed) missions %zu airfield '%s' start %d,%d,%d\n", n.c_str(),
                    t.name.c_str(), s.objectivesRemaining, s.planes, s.brownPlanes, s.crashedPlanes, t.missions.size(),
                    t.airfield.c_str(), s.startX, s.startY, s.startZ);
        CHECK(!t.name.empty());
        CHECK(s.planes + s.brownPlanes + s.crashedPlanes > 0);
        CHECK(s.objectivesRemaining > 0);
        for (const MissionInfo& m : t.missions) CHECK(!m.codeName.empty());
    }
    TheaterInfo morning;
    CHECK(loadTheaterInfo(def + "MORNING.TWR", morning));
    CHECK(morning.name == "Atkinsville (easy)");
    CHECK(morning.airfield == "RDU international Airport");
    CHECK(!morning.missions.empty() && morning.missions[0].codeName == "Insult to Injury");
    CHECK(morning.missions[0].x == 353095 && morning.missions[0].y == -138740 && morning.missions[0].z == 167038);

    if (failures == 0) std::printf("test_theaters: all passed\n");
    return failures ? 1 : 0;
}
