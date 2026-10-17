// test_flight.cpp - Flies the ported main loop headless: takeoff, climb, eject, parachute
// Needs the original DEF files (assets/original, never committed); skipped when missing.

#define SDL_MAIN_HANDLED
#include "DGroup.hpp"
#include "engine/F3dVec.hpp"
#include "game/GameState.hpp"
#include "game/Inst.hpp"
#include "game/Keyboard.hpp"
#include "game/Main3.hpp"
#include "game/TickClock.hpp"
#include "game/World.hpp"
#include "render/EgaScreen.hpp"
#include <cstdio>
#include <string>

#ifndef CORNCOB_ASSET_DIR
#define CORNCOB_ASSET_DIR "assets/original"
#endif

using namespace corncob;
namespace d = corncob::dgroup;

static int failures = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static std::string def(const char* name, const char* ext) {
    return std::string(CORNCOB_ASSET_DIR) + "/DEF/" + name + "." + ext;
}

struct Key {
    int frame;
    bool down;
    uint8_t code;
};

// Runs the game at 20 frames per second for `frames` frames with scripted keys
struct Run {
    GameState gs;
    EgaScreen scr{1};
    engine::F3dVec eng{gs, scr};
    TickClock clock;
    DosKeyboard kbd{gs, clock};
    game::World world{gs, eng};
    game::Instruments inst{gs, eng, scr, clock};
    game::Main3 game{gs, eng, world, kbd, clock, inst};
    bool ok = false;

    explicit Run(const char* theater) {
        world.cleanexp();
        ok = world.readunivfile(def(theater, "TWR"), def(theater, "CCT"));
        if (!ok) return;
        eng.readcolors();
        game.startUp(true);
    }

    // Analog stick: pitch (down = nose up) between these frames
    int padFrom = -1, padTo = -1;
    float padPitch = 0;

    void fly(int frames, const std::vector<Key>& keys) {
        size_t next = 0;
        for (int f = 0; f < frames && !game.done(); f++) {
            game.setPad(f >= padFrom && f < padTo, 0, padPitch, 0);
            const int slow = clock.advance(0.05);
            for (int i = 0; i < slow; i++) {
                if (gs.uw(d::m3::bultick)) gs.setW(d::m3::bultick, static_cast<int16_t>(gs.w(d::m3::bultick) - 1));
                if (gs.uw(d::m3::mistick)) gs.setW(d::m3::mistick, static_cast<int16_t>(gs.w(d::m3::mistick) - 1));
            }
            while (next < keys.size() && keys[next].frame <= f) {
                kbd.key(keys[next].code, keys[next].down);
                next++;
            }
            game.frame();
        }
    }
};

int main(int, char**) {
    std::FILE* probe = std::fopen(def("MORNING", "CCT").c_str(), "rb");
    if (!probe) {
        std::printf("test_flight: original DEF files not found, skipped\n");
        return 0;
    }
    std::fclose(probe);

    // The new pilot starts on foot, is put into the first plane at the strip, throttle off
    {
        Run r("MORNING");
        CHECK(r.ok);
        r.fly(30, {});
        CHECK(r.gs.b(d::m3::ejectflag) == 0);      // in the plane
        CHECK(r.gs.b(d::m3::groundflag) != 0);     // on the ground
        CHECK(r.gs.w(d::m3::rpm) == 20);           // idle
        CHECK(r.gs.w(d::m3::currpmx) == 200);      // an undamaged engine
        CHECK(r.eng.cecode == 0);
    }

    // Full throttle (keypad +), a brief back stick (keypad 2) once up to speed: takeoff and climb
    {
        Run r("MORNING");
        CHECK(r.ok);
        r.fly(500, {{4, true, 0x4E}, {20, false, 0x4E}, {270, true, 0x50}, {286, false, 0x50}});
        CHECK(!r.game.done());
        CHECK(r.eng.cecode == 0);
        CHECK(r.gs.b(d::m3::groundflag) == 0);          // airborne
        CHECK(r.gs.dw(d::m3::ox + 8) > 3000);           // and climbing
        CHECK(r.gs.w(d::m3::topv) > 5000);              // faster than 150 mph
        CHECK(r.gs.w(d::m3::orvec + 2) < 0);            // nose up
    }

    // The same takeoff with the back stick on an Xbox controller instead of the key
    {
        Run r("MORNING");
        CHECK(r.ok);
        r.padFrom = 270;
        r.padTo = 286;
        r.padPitch = 1.0f;
        r.fly(500, {{4, true, 0x4E}, {20, false, 0x4E}});
        CHECK(!r.game.done());
        CHECK(r.gs.b(d::m3::groundflag) == 0);
        CHECK(r.gs.dw(d::m3::ox + 8) > 3000);
    }

    // Eject in the air: out of the plane, the parachute opens, and the man lands standing
    {
        Run r("MORNING");
        CHECK(r.ok);
        r.fly(600, {{4, true, 0x4E}, {20, false, 0x4E}, {270, true, 0x50}, {286, false, 0x50},
                    {330, true, 0x12}, {334, false, 0x12}, {350, true, 0x39}, {352, false, 0x39}});
        CHECK(r.gs.b(d::m3::ejectflag) != 0);
        CHECK(r.gs.b(d::m3::standingflag) != 0);                 // down on his feet
        CHECK(r.gs.dw(d::m3::oz) == 720);                        // hmin above the ground
        CHECK((r.gs.uw(d::m3::missionstatus) & 0x20) != 0);      // ejected in the air
    }

    if (failures == 0) std::printf("test_flight: all passed\n");
    return failures ? 1 : 0;
}
