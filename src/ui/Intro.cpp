// Intro.cpp - Title sequence, intro story, and the loading screen

#include "Intro.hpp"
#include "../assets/Assets.hpp"
#include "../render/FrameBuffer.hpp"
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>

namespace corncob {
namespace {

enum class Step { Next, SkipAll, Quit };

struct ShotCtx {
    const char* prefix = nullptr;
    int index = 0;
};

// One scene: fade in, hold (until a key or timeout), fade out. `draw` gets the
// seconds since the scene started. Esc skips the rest of the sequence.
Step playScene(Window& win, FrameBuffer& fb, float fadeIn, float hold, float fadeOut,
               const std::function<void(float)>& draw, ShotCtx& shot) {
    if (shot.prefix) {
        // Unattended capture: render the fully faded-in frame partway into the hold
        draw(fadeIn + hold * 0.5f);
        fb.setBrightness(1.0f);
        char path[512];
        std::snprintf(path, sizeof path, "%s%02d.bmp", shot.prefix, shot.index++);
        fb.save(path);
        fb.present();
        return win.pollEvents() ? Step::Next : Step::Quit;
    }

    Uint32 start = SDL_GetTicks();
    float holdEnd = fadeIn + hold;
    bool leaving = false;
    float leaveAt = 0;
    for (;;) {
        if (!win.pollEvents()) return Step::Quit;
        float t = (SDL_GetTicks() - start) / 1000.0f;
        SDL_Scancode k = win.lastKey();
        if (k == SDL_SCANCODE_ESCAPE) return Step::SkipAll;
        if (k != SDL_SCANCODE_UNKNOWN && !leaving) {
            leaving = true;
            // Start the fade-out from the current brightness
            float cur = fadeIn > 0 ? std::min(1.0f, t / fadeIn) : 1.0f;
            leaveAt = t - (1.0f - cur) * fadeOut;
        }
        if (!leaving && hold >= 0 && t >= holdEnd) {
            leaving = true;
            leaveAt = t;
        }

        float b;
        if (leaving) {
            b = fadeOut > 0 ? 1.0f - (t - leaveAt) / fadeOut : 0.0f;
            if (b <= 0) return Step::Next;
        } else {
            b = fadeIn > 0 ? std::min(1.0f, t / fadeIn) : 1.0f;
        }
        draw(t);
        fb.setBrightness(b);
        fb.present();
    }
}

const Rgb BLACK{0, 0, 0};

// Text in a picture's palette, on black cells like BIOS int 10h/13h in mode 13h
void caption(FrameBuffer& fb, const PictLine& ln, Rgb color) {
    fb.text(ln.col, ln.row, ln.text.c_str(), color, &BLACK);
}

} // namespace

IntroResult runIntro(Window& win, const char* shotPrefix) {
    FrameBuffer fb(win.sdlRenderer(), 320, 200);
    ShotCtx shot{shotPrefix, 0};

    VgaImage earth, band, logo, logo3d;
    bool haveEarth = loadVga(assetPath("VGA/SEARTHJ.VGA"), earth);
    bool haveBand = loadVga(assetPath("VGA/TNAMBC.VGA"), band);
    bool haveLogo = loadVga(assetPath("VGA/TLOGOGR.VGA"), logo);
    bool have3d = loadVga(assetPath("VGA/BLOGOGR.VGA"), logo3d);
    std::vector<PictPage> story = loadTitleScript(assetPath("TPICT.DAT"));

    if (!haveEarth && !haveLogo && story.empty()) {
        std::cerr << "Intro assets not found under " << findAssetDir() << " - skipping intro" << std::endl;
        return IntroResult::Continue;
    }

    auto finish = [](Step s) { return s == Step::Quit ? IntroResult::Quit : IntroResult::Continue; };

    // 1. Earth from space, slowly brightening (like the fadesky ramp in SRATS.ASM)
    if (haveEarth) {
        Step s = playScene(win, fb, 2.5f, 2.0f, 1.0f, [&](float) {
            fb.clear();
            fb.blit(earth, earth.pal, (320 - earth.w) / 2, (200 - earth.h) / 2);
        }, shot);
        if (s != Step::Next) return finish(s);
    }

    // 2. Title card: CORNCOB stamped into the textured nameplate, 3D beneath it
    auto drawTitle = [&](float t) {
        fb.clear();
        int bandY = 14;
        if (haveBand) fb.blit(band, band.pal, (320 - band.w) / 2, bandY);
        if (haveLogo) {
            int lx = (320 - logo.w) / 2, ly = bandY + (100 - logo.h) / 2;
            fb.blitMask(logo, lx + 2, ly + 2, Rgb{255, 214, 150}, 0.5f); // highlight edge
            fb.blitMask(logo, lx, ly, Rgb{20, 8, 2});
        }
        if (have3d) {
            // 3D rises into place over the first second
            float k = std::min(1.0f, t / 1.0f);
            int y = static_cast<int>(200 - (200 - 120) * (1 - (1 - k) * (1 - k)));
            fb.blitMask(logo3d, (320 - logo3d.w) / 2, y, Rgb{255, 200, 60});
        }
        if (t > 2.0f && static_cast<int>(t * 2) % 2 == 0)
            fb.text(13, 24, "Press any key", Rgb{170, 170, 170});
    };
    {
        Step s = playScene(win, fb, 1.0f, 10.0f, 0.8f, drawTitle, shot);
        if (s != Step::Next) return finish(s);
    }

    // 3. The story, exactly as scripted in TPICT.DAT
    for (const PictPage& pg : story) {
        VgaImage img;
        bool ok = loadVga(assetPath(pg.file), img);
        Palette pal = ok && img.hasPalette ? img.pal : Palette{};
        if (!ok || !img.hasPalette) loadPalette(assetPath("VGA/HC6.PAL"), pal);
        Rgb color = pal.c[pg.color & 255];
        Step s = playScene(win, fb, 1.0f, 8.0f, 0.8f, [&](float) {
            fb.clear();
            if (ok) fb.blit(img, pal, pg.x, pg.y);
            for (const PictLine& ln : pg.lines) caption(fb, ln, color);
        }, shot);
        if (s == Step::Quit) return IntroResult::Quit;
        if (s == Step::SkipAll) break;
    }
    return IntroResult::Continue;
}

IntroResult runLoadingScreen(Window& win, const char* shotPrefix) {
    // Mode 10h (640x350 EGA) like the game itself
    FrameBuffer fb(win.sdlRenderer(), 640, 350);
    ShotCtx shot{shotPrefix, 0};
    const Rgb gray{170, 170, 170};

    // 3.ASM waitmsg is printed in text mode before graphics start (8x14 cells)
    Step s = playScene(win, fb, 0.0f, 1.2f, 0.0f, [&](float) {
        fb.clear();
        fb.text(0, 1, "Please wait one moment.....", gray, nullptr, 8, 14);
    }, shot);
    if (s == Step::Quit) return IntroResult::Quit;

    // Instrument panel (readimg with al=0 -> IMG\3D.IMG), faded in from black.
    // The top 200 rows are the 3D window and get drawn over by the game.
    EgaImg panel;
    if (loadImg(assetPath("IMG/3D.IMG"), panel)) {
        // No fade-out: the game takes over with the panel already on screen
        s = playScene(win, fb, 1.0f, 0.4f, 0.0f, [&](float) {
            fb.clear();
            fb.blitImg(panel, 200, EgaImg::H, gray, BLACK, false);
        }, shot);
        if (s == Step::Quit) return IntroResult::Quit;
    }
    return IntroResult::Continue;
}

} // namespace corncob
