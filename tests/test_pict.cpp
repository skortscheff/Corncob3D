// test_pict.cpp - Which pictures the end of a mission shows (PICT.ASM pict)

#define SDL_MAIN_HANDLED
#include "ui/MissionEnd.hpp"
#include <cstdio>

static int failures = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                      \
        }                                                                    \
    } while (0)

using corncob::missionPictures;
using V = std::vector<int>;

int main() {
    CHECK(missionPictures(0x4001, 0, 0).empty());                // aborted by ctrl-c: nothing
    CHECK(missionPictures(0x0400, 0, 0) == V({14}));             // lost in space
    CHECK(missionPictures(0x0010 | 0x0004, 0, 0) == V({0}));     // landed at the home base: a happy landing
    CHECK(missionPictures(0x0010 | 0x0004 | 0x0001, 0, 0).empty());   // (an abort in the air shows nothing)
    CHECK(missionPictures(0x0010 | 0x0004, 3, 0) == V({0, 6}));  // and the brig for the friendlies destroyed
    CHECK(missionPictures(0x0100 | 0x0004, 0, 0) == V({1}));     // rescued
    CHECK(missionPictures(0x0008, 0, 0) == V({8}));              // captured
    CHECK(missionPictures(0x0002, 0, 0) == V({4}));              // killed in the plane: the flyby crash
    CHECK(missionPictures(0x0002 | 0x0020, 0, 0) == V({7}));     // killed after ejecting: the dead fall
    CHECK(missionPictures(0x0040 | 0x0010, 0, 0) == V({2, 5}));  // crash-landed away from home, then got home
    CHECK(missionPictures(0x0200 | 0x0010 | 0x0004, 0, 0) == V({3}));   // crash-landed at home
    CHECK(missionPictures(0x0800 | 0x0002, 0, 0) == V({7}));     // killed by an object, unknown: dead fall
    CHECK(missionPictures(0x0800 | 0x0002, 0, 22) == V({10}));   // a shell
    CHECK(missionPictures(0x0800 | 0x0002, 0, 24) == V({11}));   // a death ball
    CHECK(missionPictures(0x0800 | 0x0002, 0, 32) == V({12}));   // a crater
    CHECK(missionPictures(0x0800 | 0x0002, 0, 40) == V({13}));   // anything else
    CHECK(missionPictures(0x0000, 0, 0) == V({9}));              // not home, not in a plane: unknown

    if (failures == 0) std::printf("test_pict: all passed\n");
    return failures ? 1 : 0;
}
