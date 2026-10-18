// MissionRunner.cpp - Flies one mission (see MissionRunner.hpp)

#include "game/MissionRunner.hpp"
#include "assets/Assets.hpp"
#include "audio/Opl.hpp"
#include "audio/Sound.hpp"
#include "core/AsmMath.hpp"
#include "engine/AsmTables.hpp"
#include "engine/F3dVec.hpp"
#include "game/GameState.hpp"
#include "game/Inst.hpp"
#include "game/Keyboard.hpp"
#include "game/Main3.hpp"
#include "game/ModernPanel.hpp"
#include "game/TickClock.hpp"
#include "game/World.hpp"
#include "platform/Input.hpp"
#include "render/EgaScreen.hpp"
#include "ui/Intro.hpp"
#include "ui/MissionEnd.hpp"
#include "ui/SpaceSequence.hpp"
#include "ui/TowerScreen.hpp"
#include "game/TheaterFiles.hpp"
#include "DGroup.hpp"

#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace corncob {

namespace d = corncob::dgroup;
using engine::i16;
using engine::u16;

namespace {

// "30+0D;60-0D": at frame 30 press the PC scan code 0D, at frame 60 release it
struct ScriptEvent {
    int frame;
    bool down;
    uint8_t code;
};

std::vector<ScriptEvent> parseScript(const char* s) {
    std::vector<ScriptEvent> out;
    while (s && *s) {
        char* end = nullptr;
        const long frame = std::strtol(s, &end, 10);
        if (end == s || (*end != '+' && *end != '-')) break;
        const bool down = *end == '+';
        const long code = std::strtol(end + 1, &end, 16);
        out.push_back({static_cast<int>(frame), down, static_cast<uint8_t>(code)});
        s = end;
        while (*s == ';' || *s == ' ' || *s == ',') s++;
    }
    return out;
}

// Xbox controller, modern dual-stick scheme with one layout for the plane and one on foot. The
// buttons press the keys of the original, so a button does what its key does.
//
//   In the plane
//     left stick    pitch (pull back = nose up) and roll
//     right stick   look: flick left / right / up for those views, down for the rear view
//                   (the original's control key + cursor keys; it springs back when released)
//     LB / RB       rudder left / right          RT / LT   throttle up / down (D-pad up / down too)
//     A guns   X missiles   B bomb   Y flaps     L3 brakes   R3 autopilot
//     D-pad left rear window   D-pad right ground detail dots
//   On foot
//     left stick    up = run forward, down = step back, left / right = strafe
//     right stick   look around (turn and tilt the view)
//     A             pull the ripcord while falling
//   Both          Back = eject / step out    Start = pause
void updatePad(const Input& in, DosKeyboard& kbd, game::Main3& game, GameState& gs, bool enabled, bool invertPitch) {
    static bool held[32] = {};
    auto key = [&](int slot, bool down, uint8_t code) {
        if (down != held[slot]) {
            held[slot] = down;
            kbd.key(code, down, 0);
        }
    };
    const bool pad = enabled && in.hasGamepad();
    auto axis = [&](SDL_GameControllerAxis a) { return pad ? in.padAxis(a) : 0.0f; };
    auto btn = [&](SDL_GameControllerButton b) { return pad && in.padButton(b); };
    const bool onFoot = gs.b(d::m3::ejectflag) != 0;
    const float lx = axis(SDL_CONTROLLER_AXIS_LEFTX), ly = axis(SDL_CONTROLLER_AXIS_LEFTY);
    const float rx = axis(SDL_CONTROLLER_AXIS_RIGHTX), ry = axis(SDL_CONTROLLER_AXIS_RIGHTY);

    game.setStrafe(onFoot ? lx : 0.0f);
    if (onFoot) game.setPad(pad, rx, -ry, 0.0f);   // turn and look (stick up = look up)
    else game.setPad(pad, lx, invertPitch ? -ly : ly, 0.0f);   // fly

    key(0, axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 0.3f || btn(SDL_CONTROLLER_BUTTON_DPAD_UP), 0x4E);      // throttle +
    key(1, axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 0.3f || btn(SDL_CONTROLLER_BUTTON_DPAD_DOWN), 0x4A);     // throttle -
    key(2, btn(SDL_CONTROLLER_BUTTON_A), 0x39);                                                              // space: guns / ripcord
    key(3, btn(SDL_CONTROLLER_BUTTON_X), 0x2E);                                                              // c: missiles
    key(4, btn(SDL_CONTROLLER_BUTTON_B), 0x30);                                                              // b: bomb
    key(5, btn(SDL_CONTROLLER_BUTTON_Y), 0x21);                                                              // f: flaps
    key(6, !onFoot && btn(SDL_CONTROLLER_BUTTON_LEFTSTICK), 0x34);                                           // .: brakes
    key(7, !onFoot && btn(SDL_CONTROLLER_BUTTON_RIGHTSTICK), 0x1E);                                          // a: autopilot
    key(8, btn(SDL_CONTROLLER_BUTTON_DPAD_LEFT), 0x13);                                                      // r: rear window
    key(9, btn(SDL_CONTROLLER_BUTTON_DPAD_RIGHT), 0x22);                                                     // g: ground dots
    key(10, btn(SDL_CONTROLLER_BUTTON_BACK), 0x12);                                                          // e: eject / step out
    key(12, btn(SDL_CONTROLLER_BUTTON_START), 0x19);                                                         // p: pause
    key(13, onFoot && ly < -0.5f, 0x4C);                                                                     // run forward
    key(14, onFoot && ly > 0.5f, 0x52);                                                                      // step back
    key(15, !onFoot && btn(SDL_CONTROLLER_BUTTON_LEFTSHOULDER), 0x2C);                                       // z: rudder left
    key(16, !onFoot && btn(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER), 0x2D);                                      // x: rudder right
    // the right stick as the look keys: control + a cursor key (or keypad 5 for behind)
    const bool flick = !onFoot && std::max(std::fabs(rx), std::fabs(ry)) > 0.6f;
    const bool horiz = std::fabs(rx) >= std::fabs(ry);
    key(17, flick, 0x1D);
    key(18, flick && horiz && rx < 0, 0x4B);
    key(19, flick && horiz && rx > 0, 0x4D);
    key(20, flick && !horiz && ry < 0, 0x48);
    key(21, flick && !horiz && ry > 0, 0x4C);
}

} // namespace

MissionOutcome runMission(Window& window, const MissionRequest& rq) {
    MissionOutcome out;
    int spawnHitFrame = rq.spawnHitFrame;
    bool spawnEnemies = rq.spawnEnemies;
    uint16_t spawnedSite = 0;

    if (rq.showLoadingScreen && !rq.shotPath) {
        if (runLoadingScreen(window) == IntroResult::Quit) {
            out.windowClosed = true;
            return out;
        }
    }

    GameState gs;
    EgaScreen screen(rq.hd);
    engine::F3dVec eng(gs, screen);
    Display display(window.sdlRenderer());
    EgaImg panel;
    game::ModernPanel modernPanel(gs);
    const bool havePanel = loadImg(assetPath("IMG/3D.IMG"), panel);
    if (havePanel) screen.drawPanel(panel, 0);

    // the options 3.EXE got on its command line (CMDLINE.ASM)
    gs.setB(d::m3::joyflag, 0);   // (the stick of a gamepad goes in through the keyboard path and setPad)
    if (rq.invincible) gs.setB(d::m3::invincible, 0xFF);
    if (rq.beginnerModel) gs.setB(d::m3::aeroflag, 0xFF);
    if (rq.comments) gs.setB(d::m3::rmsgflag, 0xFF);
    if (rq.stockPlane) gs.setB(d::m3::chainflag, 0xFF);
    if (rq.training) {
        gs.setW(d::m3::ndbst, static_cast<i16>(rq.kla));
        gs.setW(d::m3::naas, static_cast<i16>(rq.aaa));
        gs.setW(d::m3::nvest, static_cast<i16>(rq.vehicles));
        gs.setW(d::m3::qualdbst, static_cast<i16>(rq.klaQuality));
        gs.setW(d::m3::qualaa, static_cast<i16>(rq.aaaQuality));
        gs.setW(d::m3::qualvest, static_cast<i16>(rq.vehQuality));
    }

    TickClock clock;
    DosKeyboard keyboard(gs, clock);
    game::World world(gs, eng);
    world.cleanexp();
    if (!rq.training && !world.readunivfile(rq.twrPath, rq.cctPath)) {
        std::fprintf(stderr, "Can't read theater %s\n", rq.cctPath.c_str());
        return out;
    }
    eng.readcolors();
    eng.setcolors(255);

    Opl opl;   // the AdLib card
    Sound sound(gs, opl);
    if (!rq.noSound && opl.open()) sound.initsound(assetPath("3.ADL"));
    game::Instruments instruments(gs, eng, screen, clock);
    game::Main3 game(gs, eng, world, keyboard, clock, instruments);
    if (opl.isOpen()) game.setSound(&sound);
    if (rq.modernPanel)   // the smooth panel over the picture, except under the pause and help pictures
        display.setOverlay([&](const SDL_Rect& r) {
            if (!game.paused()) modernPanel.draw(window.sdlRenderer(), r, screen, eng.palette());
        });
    game.onViewChange([&](bool cockpit) {
        if (cockpit && havePanel) screen.drawPanel(panel, 0);   // readimg: the instrument panel
    });
    game.onShowImage([&](int n) {   // the help screens: 3Dn.IMG over the whole screen
        EgaImg img;
        if (loadImg(assetPath("IMG/3D" + std::to_string(n) + ".IMG"), img)) screen.drawPanel(img, 0);
    });
    game.onCrashScreen([&]() {   // 3.ASM crash: the screen fills red and shakes for 18 ticks, then a pause
        const int s = screen.scale();
        screen.fillRect(0, 0, 640 * s, 350 * s, 4);
        if (rq.shotPath) return;   // scripted runs don't wait
        for (int k = 0; k < 18 + 14; k++) {
            if (!window.pollEvents()) break;
            display.present(screen, eng.palette(), 0, 0);
            SDL_Delay(55);
        }
    });
    // the tower, the boss key and what the other F keys said in the shareware version
    TheaterInfo theaterInfo;
    const bool haveTheater = !rq.training && loadTheaterInfo(rq.twrPath, theaterInfo);
    game.onTower([&](game::Main3::TowerKind kind, bool onFoot, int32_t* start) {
        using TK = game::Main3::TowerKind;
        bool chose = false;
        switch (kind) {
        case TK::Boss: runBossScreen(window); break;
        case TK::Score: runTowerMessage(window, "Sorry, inflight score not available in this version"); break;
        case TK::Detail: runTowerMessage(window, "Sorry, inflight theater detailed info not available in this version"); break;
        case TK::Info:
            if (haveTheater) {
                const TowerChoice c = runTowerInfo(window, theaterInfo, !onFoot);
                if (c.mission) {
                    start[0] = c.x;
                    start[1] = c.y;
                    start[2] = c.z;
                    start[3] = c.yaw;
                    chose = true;
                }
            } else {
                runTowerMessage(window, "Sorry, there is only one tower in the training missions.");
            }
            break;
        }
        return chose;
    });
    game.startUp(!rq.training);

    const int minFrameTicks = static_cast<int>(TickClock::FAST_HZ / rq.fps);
    const std::vector<ScriptEvent> script = parseScript(rq.keyScript);
    size_t scriptPos = 0;

    Input input;
    input.initJoystick();

    const Uint32 startMs = SDL_GetTicks();
    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    int frame = 0;
    bool gameOver = false, running = true;

    while (running) {
        if (!window.pollEvents()) {
            out.windowClosed = true;
            break;
        }
        const Uint64 now = SDL_GetPerformanceCounter();
        // scripted runs use a fixed 60 Hz clock so that they are repeatable
        const double dt = rq.shotPath ? 1.0 / 60.0 : std::min((now - prev) / freq, 0.1);
        prev = now;

        // the timer interrupt first, then the keys that happened during the frame (new9)
        const int slow = clock.advance(dt);
        for (int i = 0; i < slow; i++) {   // new08: the 18 Hz tick counts the bullet and missile timers down
            if (gs.uw(d::m3::bultick) != 0) gs.setW(d::m3::bultick, static_cast<i16>(gs.w(d::m3::bultick) - 1));
            if (gs.uw(d::m3::mistick) != 0) gs.setW(d::m3::mistick, static_cast<i16>(gs.w(d::m3::mistick) - 1));
        }
        const Uint32 nowMs = SDL_GetTicks();
        for (const Window::KeyEvent& e : window.keyEvents()) {
            const uint8_t code = DosKeyboard::dosScanCode(e.scancode);
            const double ago = std::max(0.0, (nowMs - e.timestampMs) / 1000.0) * TickClock::FAST_HZ;
            keyboard.key(code, e.down, static_cast<uint16_t>(std::min(ago, 60000.0)));
        }
        while (scriptPos < script.size() && script[scriptPos].frame <= frame) {
            keyboard.key(script[scriptPos].code, script[scriptPos].down, 0);
            scriptPos++;
        }
        input.update();
        updatePad(input, keyboard, game, gs, rq.useGamepad, rq.invertPitch);
        if (input.isKeyJustDown(SDL_SCANCODE_F11)) {
            // HD toggle: 1x <-> 2x, panel redrawn into the new pages
            screen.setScale(screen.scale() == 1 ? 2 : 1);
            if (havePanel && gs.w(d::m3::ptrgwinf) == static_cast<i16>(d::m3::gwinf)) screen.drawPanel(panel, 0);
        }

        // The original ran at roughly 10-20 frames per second and its fixed point arithmetic is
        // tuned for that: a matrix multiply loses a few units to rounding whatever the speed, so at
        // a 60 Hz time step the thrust of one frame is smaller than that loss and the plane never
        // gets going. Frames therefore start only once enough timer ticks have passed.
        bool going = true;
        if (spawnHitFrame >= 0 && frame == 1 && !spawnedSite) {   // a site ahead of the plane (--spawn-hit)
            const uint32_t kbuf = d::m3::kbuf, ox = d::m3::ox;
            for (int k = 0; k < 9; k++) gs.setW(kbuf + 2 * k, gs.w(ox + 2 * k));
            gs.setDw(kbuf + 4, gs.dw(ox + 4) + 4000);
            gs.setDw(kbuf + 8, 0);
            world.strtperm(static_cast<uint16_t>(kbuf), static_cast<uint16_t>(d::defobjs::gms), 0, spawnedSite);
        }
        if (spawnEnemies && frame == 1) {   // one of each kind, off to the sides of the runway (--spawn-enemies)
            spawnEnemies = false;
            const uint32_t kbuf = d::m3::kbuf, ox = d::m3::ox;
            auto spot = [&](int dx, int dy) {
                for (int k = 0; k < 9; k++) gs.setW(kbuf + 2 * k, gs.w(ox + 2 * k));
                gs.setDw(kbuf, gs.dw(ox) + dx);
                gs.setDw(kbuf + 4, gs.dw(ox + 4) + dy);
                gs.setDw(kbuf + 8, 0);
            };
            uint16_t slot;
            const uint16_t src = static_cast<uint16_t>(kbuf);
            spot(6000, 3000);   world.strtaa(src, 7);
            spot(-6000, 3000);  world.strtdbst(src, 7);
            spot(7000, 8000);   world.strtvest(src, 7);
            spot(-7000, 8000);  world.strtperm(src, static_cast<uint16_t>(d::defobjs::objmort), 4, slot);
            spot(-8000, 12000); world.strtperm(src, static_cast<uint16_t>(d::defobjs::objbb), 3, slot);
            spot(8000, 12000);  world.strtperm(src, static_cast<uint16_t>(d::defobjs::objtet), 0, slot);
            spot(9000, 3000);   world.strtperm(src, static_cast<uint16_t>(d::defobjs::objbmrst), 5, slot);
        }
        if (spawnedSite && frame == spawnHitFrame && clock.frmticks >= minFrameTicks) {   // ... and its destruction
            for (int k = 0; k < 40 && gs.w(spawnedSite + 72) == 27; k++) game.objroutine(27, spawnedSite, 0xFFFF, true);
            spawnHitFrame = -1;
        }
        if (clock.frmticks >= minFrameTicks) {
            going = game.frame();
            frame++;
        }
        if (rq.shotPath && frame >= rq.shotFrames) {
            display.saveBmp(screen, eng.palette(), rq.shotPath);
            if (rq.modernPanel) modernPanel.saveComposite(screen, eng.palette(), (std::string(rq.shotPath) + ".modern.bmp").c_str());
            // the app has no console in release builds, so leave the report beside the image
            if (std::FILE* f = std::fopen((std::string(rq.shotPath) + ".txt").c_str(), "w")) {
                std::fprintf(f, "cecode=%d frame=%d ejectflag=%d groundflag=%d freezeflag=%d rpm=%d\n"
                                "ox=%d oy=%d oz=%d orvec=%d,%d,%d topv=%d,%d,%d missionstatus=%04x\n",
                             eng.cecode, frame, gs.b(d::m3::ejectflag), gs.b(d::m3::groundflag), gs.b(d::m3::freezeflag),
                             gs.w(d::m3::rpm), gs.dw(d::m3::ox), gs.dw(d::m3::ox + 4), gs.dw(d::m3::ox + 8),
                             gs.w(d::m3::orvec), gs.w(d::m3::orvec + 2), gs.w(d::m3::orvec + 4), gs.w(d::m3::topv),
                             gs.w(d::m3::topv + 2), gs.w(d::m3::topv + 4), gs.uw(d::m3::missionstatus));
                std::fprintf(f, "dvel=%d,%d,%d thrtmult=%d rhotop=%d rho0=%d rpmx=%d currpmx=%d thrtsf=%d ctopv=%d,%d,%d "
                                "alpha=%d qstald=%d nticks=%d ofrmticks=%d ozeff=%d\n",
                             gs.dw(d::m3::dvel), gs.dw(d::m3::dvel + 4), gs.dw(d::m3::dvel + 8), gs.w(d::m3::thrtmult),
                             gs.w(d::m3::rhotop), gs.w(d::m3::rho0), gs.w(d::m3::rpmx), gs.w(d::m3::currpmx),
                             gs.w(d::m3::thrsf), gs.w(d::m3::ctopv), gs.w(d::m3::ctopv + 2), gs.w(d::m3::ctopv + 4),
                             gs.w(d::m3::alpha), gs.b(d::m3::qstald), gs.w(d::m3::nticks), gs.w(d::m3::ofrmticks),
                             gs.dw(d::m3::ozeff));
                std::fclose(f);
            }
            break;
        }
        display.present(screen, eng.palette(), eng.panX(), eng.panY());
        if (!going) {
            gameOver = true;
            running = false;
        }
    }

    if (gameOver) {
        game.finishResults();
        out.over = true;
        out.status = gs.uw(d::m3::missionstatus);
        out.nfdest = gs.uw(d::m3::nfdest);
        out.deathcode = gs.uw(d::m3::deathcode);
        out.flightSeconds = static_cast<long>((SDL_GetTicks() - startMs) / 1000);
        out.result = game.resultBytes();
        if (!rq.training && !rq.shotPath) out.universe = game.saveUniverse();
        if (rq.resPath) {   // the result block the tower reads
            if (std::FILE* f = std::fopen(rq.resPath, "wb")) {
                std::fwrite(out.result.data(), 1, out.result.size(), f);
                std::fclose(f);
            }
        }
        if (out.status & 0x0400) runSpaceSequence(window);   // lost in space: SRATS
        // PICT.ASM: the picture for how it ended
        if (rq.showEndPictures) runMissionEnd(window, missionPictures(out.status, out.nfdest, out.deathcode));
    }
    return out;
}

} // namespace corncob
