// main.cpp - Corncob 3-D (Windows/SDL2 port) entry point and main loop
//
// Status: Phase 1. The intro, loading screen and instrument panel are real, and
// every frame is drawn by the ported 3D engine (F3DVEC.ASM) in the original's
// order: epage, horizon (modhr + hrobj), sun, drawnear, flipage. The observer
// is steered by a placeholder controller that writes orvec/ox directly, and a
// handful of real DEFOBJS templates stand in for the universe; Phases 2-3
// replace both with the ported 3.ASM/MAP.ASM code.

#include "assets/Assets.hpp"
#include "core/AsmMath.hpp"
#include "engine/AsmTables.hpp"
#include "engine/F3dVec.hpp"
#include "game/GameState.hpp"
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

constexpr int NBYTEXP = 74;   // F3DEQU.INC: bytes per 3D object record

// DEFOBJS templatetable indices
enum Template : int { T_HQ = 0, T_RADAR = 3, T_TW = 4, T_ROAD = 5, T_TOWER = 6, T_VECH = 12, T_HUT = 34 };

// Places a copy of a DEFOBJS template in the exps array and the near table,
// the way the universe loader will in Phase 2
class DemoScene {
public:
    DemoScene(GameState& gs, engine::F3dVec& eng) : m_g(gs), m_eng(eng) {}

    void place(int type, int32_t x, int32_t y, int32_t z, i16 yaw = 0) {
        const u16 tmpl = m_g.uw(d::defobjs::templatetable + 2 * type);
        const u16 obj = static_cast<u16>(d::strtexp::exps + m_used++ * NBYTEXP);
        std::memmove(m_g.raw() + obj, m_g.raw() + tmpl, NBYTEXP);
        m_g.setDw(obj, x);
        m_g.setDw(obj + 4, y);
        if (z != INT32_MIN) m_g.setDw(obj + 8, z);   // else keep the template's height
        m_g.setW(obj + 12, yaw);
        m_g.setB(obj + 18, 0);
        m_g.setW(obj + 72, static_cast<i16>(type));  // type index, as qtimed/checkforrunway expect
        m_eng.tbadd(static_cast<u16>(d::f3dvec::neartbl), obj);
    }

private:
    GameState& m_g;
    engine::F3dVec& m_eng;
    int m_used = 0;
};

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
void drawFrame(GameState& gs, engine::F3dVec& eng) {
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

    eng.drawnear();
    eng.flipage();
}

} // namespace

int main(int argc, char* argv[]) {
    // --screenshot <file.bmp>: skip the intro, render a few frames, save one, exit
    // --shot-intro <prefix>:   run intro + loading screen unattended, save one BMP per scene, exit
    // --skip-intro:            go straight to the loading screen and game
    // --hd <n>:                start at n x 640x350 (HD mode)
    // --pose z,yaw,pitch,roll[,fwd]: screenshot viewpoint (fwd moves along the start heading)
    const char* shotPath = nullptr;
    const char* introShots = nullptr;
    const char* pose = nullptr;
    bool skipIntro = false;
    int hd = 1;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) shotPath = argv[++i];
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
    eng.readcolors();
    eng.setcolors(255);

    Input input;
    input.initJoystick();

    // Start where 3.ASM puts the player: startcoords, moved back to the runway start
    Observer obs;
    obs.x = gs.dw(d::m3::startcoords);
    obs.y = gs.dw(d::m3::startcoords + 4) - 8000;   // ox+4 -= w*4
    obs.z = gs.dw(d::m3::startcoords + 8);
    obs.yaw = gs.w(d::m3::startcoords + 12);
    obs.pitch = gs.w(d::m3::startcoords + 14);

    // Stand-in scenery: real DEFOBJS templates laid out ahead of the start
    DemoScene scene(gs, eng);
    {
        const i16 c = engine::scos(obs.yaw), s = engine::ssin(obs.yaw);
        auto at = [&](int fwd, int side, int type, int32_t z = INT32_MIN) {
            int32_t x = obs.x + static_cast<int32_t>((static_cast<int64_t>(fwd) * c - static_cast<int64_t>(side) * s) >> 15);
            int32_t y = obs.y + static_cast<int32_t>((static_cast<int64_t>(fwd) * s + static_cast<int64_t>(side) * c) >> 15);
            scene.place(type, x, y, z, obs.yaw);
        };
        for (int f = 4000; f <= 68000; f += 16000) at(f, 0, T_ROAD, 0);
        at(20000, 9000, T_TOWER);
        at(36000, -9000, T_HUT);
        at(52000, 12000, T_RADAR);
        at(90000, -6000, T_HQ);
        at(140000, 30000, T_HQ);
        at(30000, -16000, T_VECH);
        at(70000, 20000, T_TW);
    }

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

        drawFrame(gs, eng);
        if (shotPath && ++frame == 4) {
            display.saveBmp(screen, eng.palette(), shotPath);
            break;
        }
        display.present(screen, eng.palette(), eng.panX(), eng.panY());
    }
    return 0;
}
