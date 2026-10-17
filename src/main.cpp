// main.cpp - Corncob 3-D (Windows/SDL2 port) entry point and main loop
//
// Status: Phase 3. The intro, loading screen and instrument panel are real, the universe is the
// theater the player picked (DEF/<name>.CCT and .TWR, loaded the way readunivfile does) and the
// game itself is the ported 3.ASM main loop (game/Main3): the player starts on foot at the
// airstrip and the first plane within reach takes them, flies with the original's flight model,
// and ejects, crashes and lands as the original does. Instruments, weapons, sound and the
// front end come in later phases.

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
#include "game/TickClock.hpp"
#include "game/World.hpp"
#include "math/Trig.hpp"
#include "platform/Input.hpp"
#include "platform/Window.hpp"
#include "render/EgaScreen.hpp"
#include "ui/Intro.hpp"
#include "DGroup.hpp"

#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace corncob;
namespace d = corncob::dgroup;
using engine::i16;
using engine::u16;

namespace {

// --keyscript "30+0D;60-0D": at frame 30 press the PC scan code 0D, at frame 60 release it
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
void updatePad(const Input& in, DosKeyboard& kbd, game::Main3& game, GameState& gs) {
    static bool held[32] = {};
    auto key = [&](int slot, bool down, uint8_t code) {
        if (down != held[slot]) {
            held[slot] = down;
            kbd.key(code, down, 0);
        }
    };
    const bool pad = in.hasGamepad();
    auto axis = [&](SDL_GameControllerAxis a) { return pad ? in.padAxis(a) : 0.0f; };
    auto btn = [&](SDL_GameControllerButton b) { return pad && in.padButton(b); };
    const bool onFoot = gs.b(d::m3::ejectflag) != 0;
    const float lx = axis(SDL_CONTROLLER_AXIS_LEFTX), ly = axis(SDL_CONTROLLER_AXIS_LEFTY);
    const float rx = axis(SDL_CONTROLLER_AXIS_RIGHTX), ry = axis(SDL_CONTROLLER_AXIS_RIGHTY);

    game.setStrafe(onFoot ? lx : 0.0f);
    if (onFoot) game.setPad(pad, rx, -ry, 0.0f);   // turn and look (stick up = look up)
    else game.setPad(pad, lx, ly, 0.0f);                                          // fly

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

int main(int argc, char* argv[]) {
    // --screenshot <file.bmp>: skip the intro, run the game for --frames frames, save one, exit
    // --shot-intro <prefix>:   run intro + loading screen unattended, save one BMP per scene, exit
    // --skip-intro:            go straight to the loading screen and game
    // --nosound:               no AdLib sound
    // --hd <n>:                start at n x 640x350 (HD mode)
    // --theater <name>:        DEF/<name>.CCT + .TWR to fly over (WHTSANDS, DUSK, MORNING, WASTELND)
    // --frames <n>:            frames to run before the screenshot (default 8)
    // --fps <n>:               game frames per second, 5-60 (default 20)
    // --keyscript "<f>+<hex>;<f>-<hex>": scripted key presses (PC scan codes) for screenshot runs
    // --training:              a training mission: no theater file, the permanent objects of initperms
    // --spawn-hit <n>:         a development aid: a ground missile site appears ahead of the plane at
    //                          frame 1 and is destroyed at frame n (to look at the explosion pieces)
    const char* shotPath = nullptr;
    const char* introShots = nullptr;
    const char* theater = "MORNING";
    const char* keyscript = nullptr;
    bool skipIntro = false;
    bool noSound = false;
    bool training = false;
    int hd = 1;
    int shotFrames = 8;
    int fps = 20;   // game frames per second (the original: 10-20, depending on the machine)
    int spawnHitFrame = -1;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) shotPath = argv[++i];
        else if (std::strcmp(argv[i], "--theater") == 0 && i + 1 < argc) theater = argv[++i];
        else if (std::strcmp(argv[i], "--shot-intro") == 0 && i + 1 < argc) introShots = argv[++i];
        else if (std::strcmp(argv[i], "--skip-intro") == 0) skipIntro = true;
        else if (std::strcmp(argv[i], "--nosound") == 0) noSound = true;
        else if (std::strcmp(argv[i], "--training") == 0) training = true;
        else if (std::strcmp(argv[i], "--hd") == 0 && i + 1 < argc) hd = std::clamp(std::atoi(argv[++i]), 1, 4);
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) shotFrames = std::max(1, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--fps") == 0 && i + 1 < argc) fps = std::clamp(std::atoi(argv[++i]), 5, 60);
        else if (std::strcmp(argv[i], "--keyscript") == 0 && i + 1 < argc) keyscript = argv[++i];
        else if (std::strcmp(argv[i], "--spawn-hit") == 0 && i + 1 < argc) spawnHitFrame = std::max(2, std::atoi(argv[++i]));
    }
    uint16_t spawnedSite = 0;

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
    TickClock clock;
    DosKeyboard keyboard(gs, clock);
    game::World world(gs, eng);
    world.cleanexp();
    const std::string twr = assetPath(std::string("DEF/") + theater + ".TWR");
    const std::string cct = assetPath(std::string("DEF/") + theater + ".CCT");
    if (!training && !world.readunivfile(twr, cct)) {
        std::fprintf(stderr, "Can't read theater %s (%s)\n", theater, cct.c_str());
        return 1;
    }
    eng.readcolors();
    eng.setcolors(255);

    Opl opl;                         // the AdLib card
    Sound sound(gs, opl);
    if (!noSound && opl.open()) sound.initsound(assetPath("3.ADL"));
    game::Instruments instruments(gs, eng, screen, clock);
    game::Main3 game(gs, eng, world, keyboard, clock, instruments);
    if (opl.isOpen()) game.setSound(&sound);
    game.onViewChange([&](bool cockpit) {
        if (cockpit && havePanel) screen.drawPanel(panel, 0);   // readimg: the instrument panel
    });
    game.onShowImage([&](int n) {   // the help screens: 3Dn.IMG over the whole screen
        EgaImg img;
        if (loadImg(assetPath("IMG/3D" + std::to_string(n) + ".IMG"), img)) screen.drawPanel(img, 0);
    });
    game.startUp(!training);

    const int minFrameTicks = static_cast<int>(TickClock::FAST_HZ / fps);
    const std::vector<ScriptEvent> script = parseScript(keyscript);
    size_t scriptPos = 0;

    Input input;
    input.initJoystick();

    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    int frame = 0;

    while (window.pollEvents()) {
        const Uint64 now = SDL_GetPerformanceCounter();
        // scripted runs use a fixed 60 Hz clock so that they are repeatable
        const double dt = shotPath ? 1.0 / 60.0 : std::min((now - prev) / freq, 0.1);
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
        updatePad(input, keyboard, game, gs);
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
        if (spawnHitFrame >= 0 && frame == 1 && !spawnedSite) {   // --spawn-hit: a site ahead of the plane
            const uint32_t kbuf = d::m3::kbuf, ox = d::m3::ox;
            for (int k = 0; k < 9; k++) gs.setW(kbuf + 2 * k, gs.w(ox + 2 * k));
            gs.setDw(kbuf + 4, gs.dw(ox + 4) + 4000);
            gs.setDw(kbuf + 8, 0);
            world.strtperm(static_cast<uint16_t>(kbuf), static_cast<uint16_t>(d::defobjs::gms), 0, spawnedSite);
        }
        if (spawnedSite && frame == spawnHitFrame && clock.frmticks >= minFrameTicks) {   // ... and its destruction
            for (int k = 0; k < 40 && gs.w(spawnedSite + 72) == 27; k++) game.objroutine(27, spawnedSite, 0xFFFF, true);
            spawnHitFrame = -1;
        }
        if (clock.frmticks >= minFrameTicks) {
            going = game.frame();
            frame++;
        }
        if (shotPath && frame >= shotFrames) {
            display.saveBmp(screen, eng.palette(), shotPath);
            // the app has no console in release builds, so leave the report beside the image
            if (std::FILE* f = std::fopen((std::string(shotPath) + ".txt").c_str(), "w")) {
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
        if (!going) break;
    }
    return 0;
}
