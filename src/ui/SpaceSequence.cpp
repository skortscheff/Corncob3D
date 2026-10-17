// SpaceSequence.cpp - Lost in space (see SpaceSequence.hpp)

#include "SpaceSequence.hpp"
#include "render/FrameBuffer.hpp"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace corncob {

namespace {

constexpr int FADE_FRAMES = 8 * 18, NSTARS = 768, XYRANGE = 8192, ZRANGE = 32767, ZSTEP = -128;

struct Star { int x, y, z, color; };

} // namespace

bool runSpaceSequence(Window& win, const char* shotPath) {
    FrameBuffer fb(win.sdlRenderer(), 640, 350);
    std::srand(1949);
    std::vector<Star> stars(NSTARS);
    for (Star& s : stars) {
        s.x = std::rand() % XYRANGE - XYRANGE / 2;
        s.y = std::rand() % XYRANGE - XYRANGE / 2;
        s.z = std::rand() % ZRANGE;
        s.color = std::rand() % 4;
    }
    const int lastFrame = shotPath ? FADE_FRAMES * 3 / 4 : FADE_FRAMES + 18;
    Uint32 start = SDL_GetTicks();
    for (int frame = 0; frame <= lastFrame; frame++) {
        if (!shotPath) {
            if (!win.pollEvents()) return false;
            if (win.lastKey() != SDL_SCANCODE_UNKNOWN) return true;
        } else if (frame < lastFrame) {
            for (Star& s : stars) s.z = (s.z + ZSTEP) & ZRANGE;   // run the stars up to this frame
            continue;
        }
        const int f = std::min(frame, FADE_FRAMES);
        const float k = 1.0f - static_cast<float>(f) / FADE_FRAMES;   // 1 at the start, 0 when the sky is gone
        fb.clear(Rgb{static_cast<uint8_t>(60 * k), static_cast<uint8_t>(120 * k), static_cast<uint8_t>(255 * k)});
        if (frame >= FADE_FRAMES / 2) {
            for (const Star& s : stars) {
                const int z = s.z + 1600;   // the plane the stars are projected on is 1600 away
                const int px = 320 + s.x * 640 / z, py = 175 + s.y * 456 / z;
                if (px < 0 || px >= 640 || py < 0 || py >= 350) continue;
                const uint8_t v = s.z > 26000 ? 90 : s.z > 20000 ? 170 : 255;   // dim when far
                fb.put(px, py, Rgb{v, v, v});
                if (s.z < 9000) fb.put(px + 1, py, Rgb{v, v, v});
            }
        }
        if (frame < FADE_FRAMES) {   // the Earth shrinks away
            const int sz = FADE_FRAMES - frame;
            const int radius = static_cast<int>(1023.0 * sz * sz / (FADE_FRAMES * FADE_FRAMES)) / 2 + 12;
            const int cx = 320, cy = 175 + 20;
            for (int y = -radius; y <= radius; y++) {
                const int half = static_cast<int>(std::sqrt(static_cast<double>(radius) * radius - static_cast<double>(y) * y));
                fb.fillRect(cx - half, cy + y, 2 * half + 1, 1, Rgb{0, 120, 170});
            }
        }
        if (shotPath) return fb.save(shotPath);
        for (Star& s : stars) s.z = (s.z + ZSTEP) & ZRANGE;   // zi: the stars come closer
        fb.setBrightness(1.0f);
        fb.present();
        const Uint32 due = start + static_cast<Uint32>((frame + 1) * 1000 / 18);
        const Uint32 now = SDL_GetTicks();
        if (due > now) SDL_Delay(due - now);
    }
    return true;
}

} // namespace corncob
