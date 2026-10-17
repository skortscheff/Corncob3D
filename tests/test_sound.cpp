// test_sound.cpp - The AdLib effects: register writes reach the OPL2 and make (and stop) sound

#define SDL_MAIN_HANDLED
#include "DGroup.hpp"
#include "audio/Opl.hpp"
#include "audio/Sound.hpp"
#include "game/GameState.hpp"
#include <cmath>
#include <cstdio>
#include <vector>

#ifndef CORNCOB_ASSET_DIR
#define CORNCOB_ASSET_DIR "assets/original"
#endif

using namespace corncob;
namespace lbl = corncob::lbl;

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                          \
        }                                                                        \
    } while (0)

static double rms(Opl& opl, int frames) {
    std::vector<int16_t> buf(static_cast<size_t>(frames));
    opl.render(buf.data(), frames);
    double s = 0;
    for (int16_t v : buf) s += static_cast<double>(v) * v;
    return std::sqrt(s / frames);
}

int main(int, char**) {
    GameState gs;
    Opl opl;                   // not opened: no sound card needed
    Sound snd(gs, opl);
    CHECK(gs.b(lbl::sndflg) == 0xFF);   // sound is on by default

    snd.initsound(std::string(CORNCOB_ASSET_DIR) + "/3.ADL");
    CHECK(rms(opl, 2000) < 1.0);        // silent after init

    // the engine voice: a note while it is keyed on
    snd.voiceon(0, 0x100);
    rms(opl, 2000);                     // attack
    CHECK(rms(opl, 4000) > 50.0);
    snd.voiceoff(0, 0x100);
    rms(opl, 49716);                    // a second of release
    CHECK(rms(opl, 2000) < 20.0);

    // the gun voice
    snd.guninit();
    snd.voiceoff(1, 0x200);
    snd.voiceon(1, 0x200);
    CHECK(rms(opl, 3000) > 50.0);

    // the snare of the percussion section
    snd.adlreginit();
    rms(opl, 49716);
    snd.snareinit();
    snd.snaresnd();
    CHECK(rms(opl, 3000) > 50.0);

    // sound off: writes are ignored, so nothing plays
    snd.adlreginit();
    rms(opl, 49716 * 20);               // the release of rate 3 is slow
    gs.setB(lbl::sndflg, 0);
    snd.guninit();
    snd.voiceon(1, 0x200);
    CHECK(rms(opl, 3000) < 1.0);

    if (failures == 0) std::printf("test_sound: all passed\n");
    return failures ? 1 : 0;
}
