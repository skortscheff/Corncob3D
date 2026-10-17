// main.cpp - Corncob 3-D (Windows/SDL2 port) entry point
//
// The game starts like the original's CORNCOB.BAT: the title sequence (CC3D1, CCTITLE), the menu program
// (ui/FrontEnd: MOAG), and from its menus the missions (game/MissionRunner: 3.EXE), then back to the menus.
// The command line has development aids on top of that, mostly to fly or look at one thing unattended.

#include "assets/Assets.hpp"
#include "audio/Opl.hpp"
#include "audio/RolPlayer.hpp"
#include "game/MissionRunner.hpp"
#include "math/Trig.hpp"
#include "platform/Window.hpp"
#include "ui/FrontEnd.hpp"
#include "ui/Intro.hpp"
#include "ui/MissionEnd.hpp"
#include "ui/MusicBox.hpp"
#include "ui/Scoring.hpp"
#include "ui/Settings.hpp"
#include "ui/TextScreen.hpp"
#include "ui/TowerScreen.hpp"
#include "game/TheaterFiles.hpp"

#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace corncob;

namespace {

// the music of a song file, on its own: until Esc, or rendered into a WAV file
int musicOnly(Window& window, const char* song, const char* wavOut, int wavSeconds) {
    Bnk bank;
    RolSong s;
    if (!bank.load(assetPath("CCMUSIC.BNK")) || !s.load(assetPath(song))) {
        std::fprintf(stderr, "Can't read the song or CCMUSIC.BNK\n");
        return 1;
    }
    Opl opl;
    RolPlayer player(opl, bank);
    player.start(s, wavOut == nullptr);
    if (wavOut) {   // write the first n seconds as a mono 16 bit WAV
        const int rate = Opl::sampleRate();
        const int total = rate * wavSeconds;
        std::vector<int16_t> pcm(static_cast<size_t>(total));
        int done = 0;
        while (done < total) {
            const int n = std::min(rate / 100, total - done);
            player.update(0.01);
            opl.render(pcm.data() + done, n);
            done += n;
        }
        if (std::FILE* f = std::fopen(wavOut, "wb")) {
            auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
            auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
            std::fwrite("RIFF", 1, 4, f);
            w32(36 + total * 2);
            std::fwrite("WAVEfmt ", 1, 8, f);
            w32(16); w16(1); w16(1); w32(rate); w32(rate * 2); w16(2); w16(16);
            std::fwrite("data", 1, 4, f);
            w32(total * 2);
            std::fwrite(pcm.data(), 2, pcm.size(), f);
            std::fclose(f);
        }
        return 0;
    }
    if (!opl.open()) return 1;
    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    while (window.pollEvents() && window.lastKey() != SDL_SCANCODE_ESCAPE) {
        const Uint64 now = SDL_GetPerformanceCounter();
        player.update((now - prev) / freq);
        prev = now;
        SDL_Delay(5);
    }
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    // The game:
    //   --skip-intro               go straight to the menus
    //   --nosound                  no AdLib sound in the missions (and no music)
    //   --no-music                 no music in the menus
    //   --hd <n>                   start missions at n x 640x350 (HD mode)
    //   --fps <n>                  game frames per second, 5-60 (default 20)
    //   --save-dir <dir>           where the pilots, theaters and settings are kept
    // Looking at one thing (development aids):
    //   --ui-keys "Down,Enter,x"   feed the menus these keys instead of reading the keyboard, and quit when they run out
    //   --ui-shots <prefix>        save the menu screen as <prefix>NNN.bmp each time a key is wanted
    //   --theater <name>           fly DEF/<name>.CCT + .TWR directly (WHTSANDS, DUSK, MORNING, WASTELND)
    //   --training                 fly a training mission directly (--kla n --aaa n --veh n for the enemies)
    //   --screenshot <file.bmp>    run a mission unattended for --frames frames, save one picture, exit
    //   --frames <n>               frames to run before the screenshot (default 8)
    //   --keyscript "<f>+<hex>;<f>-<hex>": scripted key presses (PC scan codes) for screenshot runs
    //   --spawn-hit <n>            a ground missile site appears ahead of the plane at frame 1 and is destroyed at frame n
    //   --spawn-enemies            one of each kind of enemy appears around the strip
    //   --end-pictures <hex status> [--death <code>] [--shot-end <prefix>]: the mission-end pictures for a status
    //   --tower-test <theater>     the tower information screen of a theater, with --ui-keys / --ui-shots
    //   --fake-mission <hex status> [--fake-kills o,f,a,s]: the menus get a canned result instead of a flight
    //   --res <file>               write the result block (3.res) there when a mission ends
    //   --shot-intro <prefix>      run the title sequence and loading screen unattended, one BMP per scene
    //   --play-rol <FILE.ROL>      play a song until Esc;  --wav-rol <FILE.ROL> <out.wav> <seconds> renders one
    MissionRequest rq;
    const char* introShots = nullptr;
    const char* theater = nullptr;
    const char* uiKeys = nullptr;
    const char* uiShots = nullptr;
    const char* towerTest = nullptr;
    const char* saveDir = nullptr;
    const char* playRol = nullptr;
    const char* wavRol = nullptr;
    const char* wavOut = nullptr;
    const char* shotEnd = nullptr;
    int fakeStatus = -1;   // --fake-mission: no flight, a canned result (for looking at the reports)
    int fakeKills[4] = {1, 25, 2, 1};
    bool skipIntro = false, noMusic = false;
    bool direct = false;
    int wavSeconds = 30;
    int endStatus = -1, deathCode = 0;
    for (int i = 1; i < argc; i++) {
        const bool more = i + 1 < argc;
        if (!std::strcmp(argv[i], "--screenshot") && more) { rq.shotPath = argv[++i]; direct = true; }
        else if (!std::strcmp(argv[i], "--theater") && more) { theater = argv[++i]; direct = true; }
        else if (!std::strcmp(argv[i], "--shot-intro") && more) introShots = argv[++i];
        else if (!std::strcmp(argv[i], "--skip-intro")) skipIntro = true;
        else if (!std::strcmp(argv[i], "--nosound")) { rq.noSound = true; noMusic = true; }
        else if (!std::strcmp(argv[i], "--no-music")) noMusic = true;
        else if (!std::strcmp(argv[i], "--training")) { rq.training = true; direct = true; }
        else if (!std::strcmp(argv[i], "--kla") && more) rq.kla = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--aaa") && more) rq.aaa = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--veh") && more) rq.vehicles = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--hd") && more) rq.hd = std::clamp(std::atoi(argv[++i]), 1, 4);
        else if (!std::strcmp(argv[i], "--frames") && more) rq.shotFrames = std::max(1, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--fps") && more) rq.fps = std::clamp(std::atoi(argv[++i]), 5, 60);
        else if (!std::strcmp(argv[i], "--keyscript") && more) rq.keyScript = argv[++i];
        else if (!std::strcmp(argv[i], "--spawn-hit") && more) rq.spawnHitFrame = std::max(2, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--spawn-enemies")) rq.spawnEnemies = true;
        else if (!std::strcmp(argv[i], "--res") && more) rq.resPath = argv[++i];
        else if (!std::strcmp(argv[i], "--save-dir") && more) saveDir = argv[++i];
        else if (!std::strcmp(argv[i], "--ui-keys") && more) uiKeys = argv[++i];
        else if (!std::strcmp(argv[i], "--ui-shots") && more) uiShots = argv[++i];
        else if (!std::strcmp(argv[i], "--tower-test") && more) towerTest = argv[++i];
        else if (!std::strcmp(argv[i], "--play-rol") && more) playRol = argv[++i];
        else if (!std::strcmp(argv[i], "--wav-rol") && i + 3 < argc) {
            wavRol = argv[++i];
            wavOut = argv[++i];
            wavSeconds = std::max(1, std::atoi(argv[++i]));
        }
        else if (!std::strcmp(argv[i], "--end-pictures") && more) endStatus = static_cast<int>(std::strtol(argv[++i], nullptr, 16));
        else if (!std::strcmp(argv[i], "--death") && more) deathCode = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--shot-end") && more) shotEnd = argv[++i];
        else if (!std::strcmp(argv[i], "--fake-mission") && more) fakeStatus = static_cast<int>(std::strtol(argv[++i], nullptr, 16));
        else if (!std::strcmp(argv[i], "--fake-kills") && more) std::sscanf(argv[++i], "%d,%d,%d,%d", &fakeKills[0], &fakeKills[1], &fakeKills[2], &fakeKills[3]);
    }
    if (saveDir) setSaveDirectory(saveDir);

    init_sin_table();

    Window window;
    if (!window.init()) return 1;

    if (playRol || wavRol) return musicOnly(window, playRol ? playRol : wavRol, wavOut, wavSeconds);

    if (endStatus >= 0) {   // just the end-of-mission pictures
        runMissionEnd(window, missionPictures(static_cast<uint16_t>(endStatus), 0, static_cast<uint16_t>(deathCode)), shotEnd);
        return 0;
    }

    if (towerTest) {   // the tower information of a theater, with the keys of --ui-keys
        TextScreen::defaultScript(uiKeys ? uiKeys : "", uiShots ? uiShots : "");
        TheaterInfo info;
        loadTheaterInfo(assetPath(std::string("DEF/") + towerTest + ".TWR"), info);
        const TowerChoice c = runTowerInfo(window, info, true);
        std::printf("tower choice: %d %d %d %d %d\n", c.mission, c.x, c.y, c.z, c.yaw);
        return 0;
    }

    if (introShots) {   // the title sequence and the loading screen, unattended
        const std::string prefix = introShots;
        if (runIntro(window, prefix.c_str()) == IntroResult::Quit) return 0;
        runLoadingScreen(window, (prefix + "load").c_str());
        return 0;
    }

    if (direct) {   // one mission, straight away
        if (!rq.training) {
            const std::string name = theater ? theater : "MORNING";
            rq.cctPath = assetPath("DEF/" + name + ".CCT");
            rq.twrPath = assetPath("DEF/" + name + ".TWR");
        }
        runMission(window, rq);
        return 0;
    }

    // the game itself: the title, then the menus
    Settings settings;
    loadSettings(settings);
    MusicBox music;
    const bool haveMusic = !noMusic && !uiKeys && music.init();
    music.setOn(settings.music);
    if (!skipIntro && !uiKeys) {
        if (haveMusic && settings.music) music.playTitle();
        if (runIntro(window) == IntroResult::Quit) return 0;
    }

    TextScreen screen(window);
    if (uiKeys) screen.script(uiKeys);
    if (uiShots) screen.shotEveryKey(uiShots);
    const MissionRequest base = rq;
    FrontEnd menus(screen, settings, haveMusic ? &music : nullptr, [&](const MissionRequest& request) {
        MissionRequest r = request;
        r.hd = base.hd;
        r.noSound = base.noSound;
        r.fps = base.fps;
        if (fakeStatus >= 0) {   // a canned result instead of a flight
            MissionOutcome out;
            out.over = true;
            out.status = static_cast<uint16_t>(fakeStatus);
            out.flightSeconds = 600;
            ResultBlock block;
            block.status = out.status;
            block.objectives = static_cast<uint16_t>(fakeKills[0]);
            block.fuelDumps = static_cast<uint16_t>(fakeKills[1]);
            block.aaaDestroyed[3] = static_cast<uint16_t>(fakeKills[2]);
            block.sitesDestroyed[2] = static_cast<uint16_t>(fakeKills[3]);
            block.maxDamage = 40;
            out.result = block.serialize();
            out.result.resize(140);
            if (!r.training) {
                std::FILE* f = std::fopen(r.cctPath.c_str(), "rb");
                if (f) {
                    std::fseek(f, 0, SEEK_END);
                    out.universe.resize(static_cast<size_t>(std::ftell(f)));
                    std::fseek(f, 0, SEEK_SET);
                    if (!out.universe.empty()) std::fread(out.universe.data(), 1, out.universe.size(), f);
                    std::fclose(f);
                }
            }
            return out;
        }
        return runMission(window, r);
    });
    menus.run();
    return 0;
}
