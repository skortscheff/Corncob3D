// test_flight.cpp - Flies the ported main loop headless: takeoff, climb, eject, parachute
// Needs the original DEF files (assets/original, never committed); skipped when missing.

#define SDL_MAIN_HANDLED
#include "DGroup.hpp"
#include "engine/F3dVec.hpp"
#include "game/GameState.hpp"
#include "audio/Opl.hpp"
#include "audio/Sound.hpp"
#include "game/Inst.hpp"
#include "game/Keyboard.hpp"
#include "game/Main3.hpp"
#include "game/TickClock.hpp"
#include "game/World.hpp"
#include "render/EgaScreen.hpp"
#include <cmath>
#include <cstdio>
#include <vector>
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
    Opl opl;
    Sound snd{gs, opl};
    bool ok = false;

    explicit Run(const char* theater, bool training = false) {
        world.cleanexp();
        ok = training || world.readunivfile(def(theater, "TWR"), def(theater, "CCT"));
        if (!ok) return;
        eng.readcolors();
        snd.initsound(std::string(CORNCOB_ASSET_DIR) + "/3.ADL");
        game.setSound(&snd);
        game.startUp(!training);
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
        // the engine is on the chip: the throttle is up, so a note is playing
        std::vector<int16_t> buf(6000);
        r.opl.render(buf.data(), 3000);   // the attack
        r.opl.render(buf.data(), 6000);
        double s = 0;
        for (int16_t v : buf) s += static_cast<double>(v) * v;
        CHECK(std::sqrt(s / 6000) > 30.0);
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

    // Map mode: the m key goes full screen and looks down from high up; m again returns to the cockpit
    {
        Run r("MORNING");
        CHECK(r.ok);
        r.fly(10, {{4, true, 4}, {5, false, 4}});   // settle on the ground
        const int32_t ox = r.gs.dw(d::m3::ox), oz = r.gs.dw(d::m3::oz);
        const int16_t cockpit = r.gs.w(d::m3::ptrgwinf);
        r.fly(8, {{2, true, 0x32}, {3, false, 0x32}});
        CHECK(r.gs.dw(d::m3::oz) == static_cast<int32_t>(210u << 16));   // 210 units up
        CHECK(r.gs.w(d::m3::ptrgwinf) == static_cast<int16_t>(d::m3::gwinfs));
        r.fly(8, {{2, true, 0x32}, {3, false, 0x32}});                    // the map key again
        CHECK(r.gs.dw(d::m3::ox) == ox);
        CHECK(r.gs.dw(d::m3::oz) == oz);                                  // back where we were
        CHECK(r.gs.w(d::m3::ptrgwinf) == cockpit);
        CHECK(r.eng.cecode == 0);
    }

    // A training mission has no theater file: initperms builds the runway, tower and the rest
    {
        Run r("MORNING", true);
        CHECK(r.ok);
        r.fly(20, {});
        int objects = 0;
        for (int k = 0; k < 320; k++)   // the near object table
            if (r.gs.w(d::strtexp::exps + 74u * k + 24) != 0) objects++;
        CHECK(objects > 10);                       // the near ones: road pieces, tower, hut ... (the rest are farmed out)
        CHECK(r.gs.b(d::m3::ejectflag) == 0);      // in the plane
        CHECK(r.eng.cecode == 0);
    }

    // A ground missile site destroyed: flotsam and shards fly (objmov), the flotsam burns out, the
    // shards stay on the ground, the wreck is a smoking non-destructable thing, a boom is sounded
    {
        Run r("MORNING");
        CHECK(r.ok);
        r.fly(30, {});
        auto countType = [&](int type) {
            int n = 0;
            for (int k = 0; k < 320; k++) {
                const uint32_t o = d::strtexp::exps + 74u * k;
                if (r.gs.w(o + 24) != 0 && r.gs.w(o + 72) == type) n++;
            }
            return n;
        };
        uint16_t site = 0;
        CHECK(!r.world.strtperm(static_cast<uint16_t>(d::m3::ox), static_cast<uint16_t>(d::defobjs::gms), 0, site));
        r.fly(2, {});   // joins the near table
        CHECK(r.gs.w(site + 72) == 27);
        const int ndest = r.gs.w(d::m3::ndest);
        for (int k = 0; k < 40 && r.gs.w(site + 72) == 27; k++) r.game.objroutine(27, site, 0xFFFF, true);
        CHECK(r.gs.w(site + 72) == 33);                       // a smoking thing now
        CHECK(r.gs.w(site + 24) == 0x11);
        CHECK(r.gs.w(d::m3::ndest) == ndest + 1);
        const int flotsam = countType(25) + countType(26);
        CHECK(flotsam > 0);
        CHECK(countType(8) + countType(9) == 0);              // (the site makes no shards)
        // a friendly (the radar) falls apart into flotsam and shards
        uint16_t radar = 0;
        CHECK(!r.world.strtperm(static_cast<uint16_t>(d::m3::ox), static_cast<uint16_t>(d::defobjs::objrdr), 0, radar));
        r.fly(2, {});   // (it sits on the plane, which may already have flattened it)
        for (int k = 0; k < 40 && r.gs.w(radar + 72) == 3; k++) r.game.objroutine(3, radar, 0xFFFF, true);
        CHECK(r.gs.w(radar + 72) == 33);
        CHECK(countType(8) + countType(9) > 0);
        r.fly(1, {});
        CHECK(r.eng.cecode == 0);
        r.fly(250, {});                                       // 12 seconds later
        CHECK(countType(25) + countType(26) == 0);            // the flotsam is gone
        CHECK(countType(8) + countType(9) > 0);               // the shards lie where they fell
        CHECK(r.eng.cecode == 0);
    }

    // The weapons: the guns (space), a missile (c), a bomb (b)
    {
        Run r("MORNING");
        CHECK(r.ok);
        auto countType = [&](int type) {
            int n = 0;
            for (int k = 0; k < 320; k++) {
                const uint32_t o = d::strtexp::exps + 74u * k;
                if (r.gs.w(o + 24) != 0 && r.gs.w(o + 72) == type) n++;
            }
            return n;
        };
        auto liveBullets = [&]() {
            int n = 0;
            for (uint32_t o = d::defobjs::bullets + 74; o < d::defobjs::bptrmx; o += 74)
                if (r.gs.w(o + 24) != 0) n++;
            return n;
        };
        r.fly(30, {});
        r.fly(10, {{0, true, 0x39}, {9, false, 0x39}});   // the space bar
        CHECK(liveBullets() > 0);
        CHECK(r.gs.w(d::m3::bulavg) > 0);
        r.fly(6, {{0, true, 0x2E}, {5, false, 0x2E}});    // 'c': a missile
        CHECK(countType(21) > 0);
        CHECK(r.gs.w(d::m3::misavg) > 0);
        const int bombs = r.gs.b(d::m3::nbombs);
        r.fly(4, {{0, true, 0x30}, {1, false, 0x30}});    // 'b': a bomb, on the key release
        CHECK(countType(20) > 0);
        CHECK(r.gs.b(d::m3::nbombs) == bombs - 1);
        r.fly(300, {});                                    // the bomb falls and blows up; the bullets expire
        CHECK(countType(20) == 0);
        CHECK(countType(32) > 0);                          // a crater where it fell
        CHECK(liveBullets() == 0);
        CHECK(r.eng.cecode == 0);
    }

    // A training mission: the sites, guns and balls of initperms all run their routines for a while
    {
        Run r("MORNING", true);
        CHECK(r.ok);
        auto countType = [&](int type) {
            int n = 0;
            for (int k = 0; k < 320; k++) {
                const uint32_t o = d::strtexp::exps + 74u * k;
                if (r.gs.w(o + 24) != 0 && r.gs.w(o + 72) == type) n++;
            }
            return n;
        };
        // full throttle, off the strip, climbing: everything gets its turn
        r.fly(900, {{4, true, 0x4E}, {20, false, 0x4E}, {270, true, 0x50}, {286, false, 0x50}});
        std::printf("training: frame ok, cecode=%d gms=%d aa=%d vest=%d dbst=%d dball=%d vech=%d mort=%d\n", r.eng.cecode,
                    countType(27), countType(23), countType(13), countType(11), countType(24), countType(12), countType(42));
        CHECK(r.eng.cecode == 0);
    }

    // The enemies, each placed off to the side of the runway: their routines must run without errors, and the
    // ones that act do so (the aa guns shoot shells, the sites make death balls and vehicles, mortars fire)
    {
        Run r("MORNING", true);
        CHECK(r.ok);
        auto countType = [&](int type) {
            int n = 0;
            for (int k = 0; k < 320; k++) {
                const uint32_t o = d::strtexp::exps + 74u * k;
                if (r.gs.w(o + 24) != 0 && r.gs.w(o + 72) == type) n++;
            }
            return n;
        };
        r.fly(10, {});
        const uint32_t kbuf = d::m3::kbuf, ox = d::m3::ox;
        auto spot = [&](int dxPos, int dyPos) {   // a position near the plane (x and y offsets in units)
            for (int k = 0; k < 9; k++) r.gs.setW(kbuf + 2 * k, r.gs.w(ox + 2 * k));
            r.gs.setDw(kbuf, r.gs.dw(ox) + dxPos);
            r.gs.setDw(kbuf + 4, r.gs.dw(ox + 4) + dyPos);
            r.gs.setDw(kbuf + 8, 0);
        };
        uint16_t slot = 0;
        spot(12000, 3000);
        CHECK(!r.world.strtaa(static_cast<uint16_t>(kbuf), 7));
        spot(-12000, 3000);
        CHECK(!r.world.strtdbst(static_cast<uint16_t>(kbuf), 7));
        spot(14000, 8000);
        CHECK(!r.world.strtvest(static_cast<uint16_t>(kbuf), 7));
        spot(-14000, 8000);
        CHECK(!r.world.strtperm(static_cast<uint16_t>(kbuf), static_cast<uint16_t>(d::defobjs::objmort), 4, slot));
        spot(-16000, 12000);
        CHECK(!r.world.strtperm(static_cast<uint16_t>(kbuf), static_cast<uint16_t>(d::defobjs::objbb), 3, slot));
        spot(16000, 12000);
        CHECK(!r.world.strtperm(static_cast<uint16_t>(kbuf), static_cast<uint16_t>(d::defobjs::objtet), 0, slot));
        spot(18000, 3000);
        CHECK(!r.world.strtperm(static_cast<uint16_t>(kbuf), static_cast<uint16_t>(d::defobjs::objbmrst), 5, slot));
        int shells = 0, balls = 0, vechs = 0, flot = 0, bmrs = 0;
        for (int round = 0; round < 80; round++) {
            r.fly(20, {});
            shells = std::max(shells, countType(22));
            balls = std::max(balls, countType(24));
            vechs = std::max(vechs, countType(12));
            flot = std::max(flot, countType(25) + countType(26));
            bmrs = std::max(bmrs, countType(50));
            if (r.game.done()) break;
        }
        std::printf("enemies: shells=%d balls=%d vechs=%d flotsam=%d bombers=%d cecode=%d done=%d\n", shells, balls, vechs,
                    flot, bmrs, r.eng.cecode, r.game.done() ? 1 : 0);
        CHECK(r.eng.cecode == 0);
        CHECK(shells > 0);
        CHECK(balls > 0);
        CHECK(vechs > 0);
    }
    // A long flight over the real theaters with the guns going, and a bomb or a missile now and then: the
    // enemy routines of the whole universe get their turn, and nothing raises an error
    for (const char* theater : {"MORNING", "DUSK", "WASTELND"}) {
        Run r(theater);
        CHECK(r.ok);
        r.gs.setB(d::m3::invincible, 0xFF);   // keeps it flying (and bouncing) over the enemies
        std::vector<Key> keys = {{4, true, 0x4E}, {20, false, 0x4E}, {270, true, 0x50}, {286, false, 0x50}, {330, true, 0x1E}, {332, false, 0x1E}};   // takeoff, then the autopilot
        for (int f = 300; f < 6000; f += 40) {
            keys.push_back({f, true, 0x39});        // the guns
            keys.push_back({f + 15, false, 0x39});
            keys.push_back({f + 20, true, (f / 40) % 3 == 0 ? uint8_t(0x30) : uint8_t(0x2E)});   // a bomb, or a missile
            keys.push_back({f + 22, false, (f / 40) % 3 == 0 ? uint8_t(0x30) : uint8_t(0x2E)});
        }
        r.fly(6000, keys);
        std::printf("soak %s: cecode=%d done=%d status=%04x\n", theater, r.eng.cecode, r.game.done() ? 1 : 0,
                    r.gs.uw(d::m3::missionstatus));
        CHECK(r.eng.cecode == 0);
    }

    if (failures == 0) std::printf("test_flight: all passed\n");
    return failures ? 1 : 0;
}
