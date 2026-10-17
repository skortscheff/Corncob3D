// main.cpp - Corncob 3-D (Windows/SDL2 port) entry point and main loop
//
// Status: Phase 2. The intro, loading screen and instrument panel are real, the
// universe is the theater the player picked (DEF/<name>.CCT and .TWR, loaded the
// way readunivfile does) and every frame is drawn by the ported 3D engine in the
// original's order: epage, horizon (modhr + hrobj), sun, drawmemseg, expmov,
// drawnear, copytable, dotiles, flipage. The observer is steered by a placeholder
// controller that writes orvec/ox directly; Phase 3 replaces it with 3.ASM.

#include "assets/Assets.hpp"
#include "core/AsmMath.hpp"
#include "engine/AsmTables.hpp"
#include "engine/F3dVec.hpp"
#include "game/GameState.hpp"
#include "game/World.hpp"
#include "math/Trig.hpp"
#include "platform/Input.hpp"
#include "platform/Window.hpp"
#include "render/EgaScreen.hpp"
#include "ui/Intro.hpp"
#include "DGroup.hpp"

#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

using namespace corncob;
namespace d = corncob::dgroup;
using engine::i16;
using engine::u16;

namespace {

// Placeholder for the 3.ASM flight model: drives orvec (yaw, pitch, roll) and
// moves the eye along the view direction
struct Observer {
    int32_t x = 0, y = 0, z = 0;
    i16 yaw = 0, pitch = 0, roll = 0;
    int speed = 0;   // world units per second
};

void steer(Observer& o, const Input& in, float dt) {
    const float rate = 12000.0f * dt;   // binary angle units per second
    if (in.isKeyDown(SDL_SCANCODE_UP)) o.pitch = static_cast<i16>(o.pitch + rate);
    if (in.isKeyDown(SDL_SCANCODE_DOWN)) o.pitch = static_cast<i16>(o.pitch - rate);
    if (in.isKeyDown(SDL_SCANCODE_LEFT)) o.roll = static_cast<i16>(o.roll - rate);
    if (in.isKeyDown(SDL_SCANCODE_RIGHT)) o.roll = static_cast<i16>(o.roll + rate);
    if (in.isKeyDown(SDL_SCANCODE_COMMA)) o.yaw = static_cast<i16>(o.yaw + rate);
    if (in.isKeyDown(SDL_SCANCODE_PERIOD)) o.yaw = static_cast<i16>(o.yaw - rate);
    if (in.isKeyDown(SDL_SCANCODE_A)) o.speed = std::min(o.speed + static_cast<int>(20000 * dt), 40000);
    if (in.isKeyDown(SDL_SCANCODE_Z)) o.speed = std::max(o.speed - static_cast<int>(20000 * dt), 0);
    if (in.hasJoystick()) {
        o.roll = static_cast<i16>(o.roll + in.joystickAxis(0) * rate);
        o.pitch = static_cast<i16>(o.pitch - in.joystickAxis(1) * rate);
    }
}

void moveObserver(Observer& o, GameState& gs, float dt) {
    // forward = row 0 of orot: (cos y cos p, sin y cos p, -sin p) in 1.15
    const u16 orot = static_cast<u16>(d::f3dvec::orot);
    const float step = o.speed * dt / 32768.0f;
    o.x += static_cast<int32_t>(gs.w(orot) * step);
    o.y += static_cast<int32_t>(gs.w(orot + 2) * step);
    o.z += static_cast<int32_t>(gs.w(orot + 4) * step);
    o.z = std::max(o.z, 720);   // ehmin, the lowest the eye goes
}

// 3.ASM: orvec -> orot/norot (calcsin, calcmat, ncalcmat), ox/oy/oz, viewkludge
void applyObserver(const Observer& o, GameState& gs, engine::F3dVec& eng) {
    const u16 orvec = static_cast<u16>(d::m3::orvec);
    gs.setW(orvec, o.yaw);
    gs.setW(orvec + 2, o.pitch);
    gs.setW(orvec + 4, o.roll);
    eng.calcsin(orvec);
    eng.calcmat(static_cast<u16>(d::f3dvec::orot));
    eng.calcsin(orvec);
    eng.ncalcmat(static_cast<u16>(d::f3dvec::norot));
    gs.setDw(d::m3::ox, o.x);
    gs.setDw(d::m3::ox + 4, o.y);
    gs.setDw(d::m3::ox + 8, o.z);
    gs.setW(d::m3::viewkludge, o.pitch);
}

// One frame in the order of the 3.ASM main loop
void drawFrame(GameState& gs, engine::F3dVec& eng, game::World& world) {
    const u16 ptrgwinf = gs.uw(d::m3::ptrgwinf);
    gs.setW(d::m3::frmnum, static_cast<i16>(gs.w(d::m3::frmnum) + 1));
    eng.epage();

    eng.modhr();
    gs.setB(d::m3::hrflg, 0xFF);
    eng.d3dobj(static_cast<u16>(d::m3::hrobj), ptrgwinf);
    gs.setB(d::m3::hrflg, 0);

    // the sun: 1000 units out at sunangle above the horizon
    const u16 xvec = static_cast<u16>(d::m3::xvec);
    const i16 sunangle = gs.w(d::m3::sunangle);
    gs.setW(xvec, 0);
    gs.setW(xvec + 2, asmx::hmul(static_cast<i16>(-engine::ssin(sunangle)), 1000));
    gs.setW(xvec + 4, asmx::hmul(engine::scos(sunangle), 1000));
    eng.drawrdirect(static_cast<u16>(d::defobjs::objsun), ptrgwinf);

    world.drawmemseg();      // med/far objects; the ones that came close join the near table
    world.expmovPrime();     // STRTEXP expmov: primed objects are added to the near table
    eng.drawnear();
    world.copytable();       // objects that moved away go back to the med/far table
    world.dotiles();         // read in / farm out tiles around the observer
    eng.flipage();
}

} // namespace

int main(int argc, char* argv[]) {
    // --screenshot <file.bmp>: skip the intro, render a few frames, save one, exit
    // --shot-intro <prefix>:   run intro + loading screen unattended, save one BMP per scene, exit
    // --skip-intro:            go straight to the loading screen and game
    // --hd <n>:                start at n x 640x350 (HD mode)
    // --theater <name>:        DEF/<name>.CCT + .TWR to fly over (WHTSANDS, DUSK, MORNING, WASTELND)
    // --pose z,yaw,pitch,roll[,fwd]: screenshot viewpoint (fwd moves along the start heading)
    const char* shotPath = nullptr;
    const char* introShots = nullptr;
    const char* pose = nullptr;
    const char* theater = "MORNING";
    bool skipIntro = false;
    int hd = 1;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) shotPath = argv[++i];
        else if (std::strcmp(argv[i], "--theater") == 0 && i + 1 < argc) theater = argv[++i];
        else if (std::strcmp(argv[i], "--pose") == 0 && i + 1 < argc) pose = argv[++i];
        else if (std::strcmp(argv[i], "--shot-intro") == 0 && i + 1 < argc) introShots = argv[++i];
        else if (std::strcmp(argv[i], "--skip-intro") == 0) skipIntro = true;
        else if (std::strcmp(argv[i], "--hd") == 0 && i + 1 < argc) hd = std::clamp(std::atoi(argv[++i]), 1, 4);
    }

    init_sin_table();

    Window window;
    if (!window.init()) return 1;

    if (!shotPath) {
        std::string prefix = introShots ? introShots : "";
        if (!skipIntro && runIntro(window, introShots ? prefix.c_str() : nullptr) == IntroResult::Quit)
            return 0;
        std::string loadPrefix = prefix + "load";
        if (runLoadingScreen(window, introShots ? loadPrefix.c_str() : nullptr) == IntroResult::Quit)
            return 0;
        if (introShots) return 0;
    }

    GameState gs;
    EgaScreen screen(hd);
    engine::F3dVec eng(gs, screen);
    Display display(window.sdlRenderer());
    EgaImg panel;
    const bool havePanel = loadImg(assetPath("IMG/3D.IMG"), panel);
    if (havePanel) screen.drawPanel(panel, 0);

    // The theater MOAG would have left as towers.dat/3univ.dat (3.ASM readunivfile)
    game::World world(gs, eng);
    world.cleanexp();
    const std::string twr = assetPath(std::string("DEF/") + theater + ".TWR");
    const std::string cct = assetPath(std::string("DEF/") + theater + ".CCT");
    if (!world.readunivfile(twr, cct)) {
        std::fprintf(stderr, "Can't read theater %s (%s)\n", theater, cct.c_str());
        return 1;
    }

    eng.readcolors();
    eng.setcolors(255);
    {   // if the sky is dark, make the ground detail light and the sun a moon
        const u16 sky = static_cast<u16>(d::f3dvec::skycolors);
        if (gs.b(sky) + gs.b(sky + 1) + gs.b(sky + 2) <= 0x30) {
            gs.setB(d::m3::gndcolor, 0x0E);
            gs.setB(d::defobjs::sunclr, 15);
        }
    }

    Input input;
    input.initJoystick();

    // Start where 3.ASM puts a player who comes from the menu: on foot at startcoords,
    // 3000 units back along y
    Observer obs;
    obs.x = gs.dw(d::m3::startcoords);
    obs.y = gs.dw(d::m3::startcoords + 4) - 3000;
    obs.z = gs.dw(d::m3::startcoords + 8);
    obs.yaw = gs.w(d::m3::startcoords + 12);
    obs.pitch = gs.w(d::m3::startcoords + 14);

    if (shotPath) {
        // a little above the field, banked, looking down the line of objects
        obs.z = 4000;
        obs.pitch = 900;
        obs.roll = 1500;
        if (pose) {
            int z = obs.z, yaw = obs.yaw, pitch = obs.pitch, roll = obs.roll, fwd = 0;
            std::sscanf(pose, "%d,%d,%d,%d,%d", &z, &yaw, &pitch, &roll, &fwd);
            const i16 c = engine::scos(obs.yaw), s = engine::ssin(obs.yaw);
            obs.x += static_cast<int32_t>((static_cast<int64_t>(fwd) * c) >> 15);
            obs.y += static_cast<int32_t>((static_cast<int64_t>(fwd) * s) >> 15);
            obs.z = z;
            obs.yaw = static_cast<i16>(yaw);
            obs.pitch = static_cast<i16>(pitch);
            obs.roll = static_cast<i16>(roll);
        }
    }

    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    int frame = 0;

    while (window.pollEvents()) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = std::min(static_cast<float>((now - prev) / freq), 0.1f);
        prev = now;

        input.update();
        if (input.isKeyJustDown(SDL_SCANCODE_ESCAPE)) break;
        if (input.isKeyJustDown(SDL_SCANCODE_H)) {
            // HD toggle: 1x <-> 2x, panel redrawn into the new pages
            screen.setScale(screen.scale() == 1 ? 2 : 1);
            if (havePanel) screen.drawPanel(panel, 0);
        }
        if (input.isKeyJustDown(SDL_SCANCODE_T)) {
            // tail (rear view) window on/off
            gs.setW(d::m3::ptrgwint, gs.w(d::m3::ptrgwint) ? 0 : static_cast<i16>(d::m3::gwint));
            if (havePanel) screen.drawPanel(panel, 0);
        }
        if (input.isKeyJustDown(SDL_SCANCODE_F)) gs.setB(d::m3::fillflg, gs.b(d::m3::fillflg) ? 0 : 0xFF);
        if (input.isKeyJustDown(SDL_SCANCODE_R)) obs.roll = obs.pitch = 0;

        steer(obs, input, dt);
        applyObserver(obs, gs, eng);
        moveObserver(obs, gs, dt);
        applyObserver(obs, gs, eng);

        drawFrame(gs, eng, world);
        if (shotPath && ++frame == 8) {   // dotiles, drawmemseg and the near table need a few frames
            display.saveBmp(screen, eng.palette(), shotPath);
            // the app has no console in release builds, so leave the report beside the image
            if (std::FILE* f = std::fopen((std::string(shotPath) + ".txt").c_str(), "w")) {
                std::fprintf(f, "cecode=%d nearobjects=%d\n", eng.cecode, gs.w(gs.uw(d::f3dvec::tblist)));
                std::fclose(f);
            }
            break;
        }
        display.present(screen, eng.palette(), eng.panX(), eng.panY());
    }
    return 0;
}
