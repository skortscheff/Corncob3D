// MissionEnd.cpp - The pictures shown when a mission ends (PICT.ASM pict)

#include "MissionEnd.hpp"
#include "../assets/Assets.hpp"
#include "../render/FrameBuffer.hpp"
#include <SDL.h>
#include <algorithm>
#include <cstdio>

namespace corncob {

IntroResult runMissionEnd(Window& win, const std::vector<int>& pictures, const char* shotPrefix) {
    if (pictures.empty()) return IntroResult::Continue;
    const std::vector<PictPage> script = loadMissionScript(assetPath("PICT.DAT"));
    FrameBuffer fb(win.sdlRenderer(), 320, 200);
    int shotIndex = 0;
    for (const int number : pictures) {
        if (number < 0 || number >= static_cast<int>(script.size())) continue;
        const PictPage& pg = script[number];
        VgaImage img;
        const bool ok = loadVga(assetPath(pg.file), img);
        Palette pal = ok && img.hasPalette ? img.pal : Palette{};
        if (!ok || !img.hasPalette) loadPalette(assetPath("VGA/HC6.PAL"), pal);
        const Rgb color = pal.c[pg.color & 255];
        auto draw = [&]() {
            fb.clear();
            if (ok) fb.blit(img, pal, pg.x, pg.y);
            static const Rgb black{0, 0, 0};
            for (const PictLine& ln : pg.lines) fb.text(ln.col, ln.row, ln.text.c_str(), color, &black);
        };
        if (shotPrefix) {   // unattended: one frame per picture
            draw();
            fb.setBrightness(1.0f);
            char path[512];
            std::snprintf(path, sizeof path, "%s%02d.bmp", shotPrefix, shotIndex++);
            fb.save(path);
            continue;
        }
        const Uint32 start = SDL_GetTicks();
        bool leaving = false;
        float leaveAt = 0;
        for (;;) {   // fade in, wait for a key, fade out
            if (!win.pollEvents()) return IntroResult::Quit;
            const float t = (SDL_GetTicks() - start) / 1000.0f;
            const SDL_Scancode k = win.lastKey();
            if (k == SDL_SCANCODE_ESCAPE) return IntroResult::Continue;
            if (k != SDL_SCANCODE_UNKNOWN && t > 0.3f && !leaving) {
                leaving = true;
                leaveAt = t;
            }
            float b = std::min(1.0f, t / 0.6f);
            if (leaving) {
                b = 1.0f - (t - leaveAt) / 0.5f;
                if (b <= 0) break;
            }
            draw();
            fb.setBrightness(b);
            fb.present();
        }
    }
    return IntroResult::Continue;
}

} // namespace corncob
