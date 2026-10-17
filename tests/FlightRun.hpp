// FlightRun.hpp - Flies the ported main loop headless, with scripted keys, for the flight tests
// Needs the original DEF files (assets/original, never committed).

#pragma once
#define SDL_MAIN_HANDLED
#include "DGroup.hpp"
#include "audio/Opl.hpp"
#include "audio/Sound.hpp"
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
#include <vector>

#ifndef CORNCOB_ASSET_DIR
#define CORNCOB_ASSET_DIR "assets/original"
#endif

namespace d = corncob::dgroup;
static std::string def(const char* name, const char* ext) {
    return std::string(CORNCOB_ASSET_DIR) + "/DEF/" + name + "." + ext;
}

struct Key {
    int frame;
    bool down;
    uint8_t code;
};

// A key at a time in seconds (for runs at other frame rates)
struct TimedKey {
    double t;
    bool down;
    uint8_t code;
};

using namespace corncob;

// Runs the game at `fps` frames per second (20 by default) with scripted keys
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

    double fps = 20;   // the frame rate of the run

    explicit Run(const char* theater, bool training = false, double frameRate = 20) : fps(frameRate) {
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
            const int slow = clock.advance(1.0 / fps);
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

    // `seconds` of flight with keys given in seconds, whatever the frame rate
    void flyTimed(double seconds, const std::vector<TimedKey>& keys) {
        size_t next = 0;
        const int frames = static_cast<int>(seconds * fps + 0.5);
        for (int f = 0; f < frames && !game.done(); f++) {
            const double now = f / fps;
            game.setPad(false, 0, 0, 0);
            const int slow = clock.advance(1.0 / fps);
            for (int i = 0; i < slow; i++) {
                if (gs.uw(d::m3::bultick)) gs.setW(d::m3::bultick, static_cast<int16_t>(gs.w(d::m3::bultick) - 1));
                if (gs.uw(d::m3::mistick)) gs.setW(d::m3::mistick, static_cast<int16_t>(gs.w(d::m3::mistick) - 1));
            }
            while (next < keys.size() && keys[next].t <= now + 1e-9) {
                kbd.key(keys[next].code, keys[next].down);
                next++;
            }
            game.frame();
        }
    }
};
