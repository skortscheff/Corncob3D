// FrameBuffer.hpp - Software framebuffer for the title/story/loading screens
// Emulates mode 13h (320x200) or mode 10h (640x350) output in truecolor, with a
// global brightness factor standing in for the original DAC palette fades
// (setcolors in COLORS.ASM, fadesky in SRATS.ASM).

#pragma once
#include "../assets/Assets.hpp"
#include <SDL.h>
#include <cstdint>
#include <vector>

namespace corncob {

class FrameBuffer {
public:
    FrameBuffer(SDL_Renderer* sdl, int w, int h);
    ~FrameBuffer();
    FrameBuffer(const FrameBuffer&) = delete;
    FrameBuffer& operator=(const FrameBuffer&) = delete;

    int width() const { return m_w; }
    int height() const { return m_h; }

    void clear(Rgb c = {0, 0, 0});
    void put(int x, int y, Rgb c);
    void fillRect(int x, int y, int w, int h, Rgb c);

    // Indexed image through a palette; transparent index skipped if >= 0
    void blit(const VgaImage& img, const Palette& pal, int x, int y, int transparent = -1);
    // Logo masks: pixel values 32..224 are shade levels (32 = background)
    void blitMask(const VgaImage& img, int x, int y, Rgb ink, float strength = 1.0f);
    // 1-bit EGA image (rows [y0, y1)), set bits drawn in `on`, clear bits in `off` unless offTransparent
    void blitImg(const EgaImg& img, int y0, int y1, Rgb on, Rgb off, bool offTransparent);

    // BIOS-style text: 8x8 glyphs in cellW x cellH cells (8x8 in mode 13h, 8x14 in mode 10h)
    void text(int col, int row, const char* s, Rgb fg, const Rgb* bg = nullptr, int cellW = 8, int cellH = 8);
    void textPx(int x, int y, const char* s, Rgb fg, int scale = 1);

    // 0 = black, 1 = full brightness
    void setBrightness(float b) { m_bright = b; }

    // Upload, scale to a 4:3 letterbox in the window, and present
    void present();
    // Save the (brightness-adjusted) frame as BMP, for testing
    bool save(const char* path) const;

private:
    SDL_Renderer* m_sdl;
    SDL_Texture* m_tex = nullptr;
    int m_w, m_h;
    std::vector<uint32_t> m_px; // ARGB8888
    float m_bright = 1.0f;
};

} // namespace corncob
