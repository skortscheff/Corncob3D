// FrameBuffer.cpp - Software framebuffer for the title/story/loading screens

#include "FrameBuffer.hpp"
#include "Font8x8.hpp"
#include <algorithm>

namespace corncob {

static uint32_t pack(Rgb c) { return 0xFF000000u | (c.r << 16) | (c.g << 8) | c.b; }

FrameBuffer::FrameBuffer(SDL_Renderer* sdl, int w, int h)
    : m_sdl(sdl), m_w(w), m_h(h), m_px(static_cast<size_t>(w) * h, 0xFF000000u) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    m_tex = SDL_CreateTexture(m_sdl, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
}

FrameBuffer::~FrameBuffer() {
    if (m_tex) SDL_DestroyTexture(m_tex);
}

void FrameBuffer::clear(Rgb c) {
    std::fill(m_px.begin(), m_px.end(), pack(c));
}

void FrameBuffer::put(int x, int y, Rgb c) {
    if (x >= 0 && y >= 0 && x < m_w && y < m_h) m_px[y * m_w + x] = pack(c);
}

void FrameBuffer::fillRect(int x, int y, int w, int h, Rgb c) {
    for (int j = std::max(0, y); j < std::min(m_h, y + h); j++)
        for (int i = std::max(0, x); i < std::min(m_w, x + w); i++)
            m_px[j * m_w + i] = pack(c);
}

void FrameBuffer::blit(const VgaImage& img, const Palette& pal, int x, int y, int transparent) {
    for (int j = 0; j < img.h; j++) {
        int sy = y + j;
        if (sy < 0 || sy >= m_h) continue;
        for (int i = 0; i < img.w; i++) {
            int sx = x + i;
            if (sx < 0 || sx >= m_w) continue;
            uint8_t v = img.px[j * img.w + i];
            if (v == transparent) continue;
            m_px[sy * m_w + sx] = pack(pal.c[v]);
        }
    }
}

void FrameBuffer::blitMask(const VgaImage& img, int x, int y, Rgb ink, float strength) {
    for (int j = 0; j < img.h; j++) {
        int sy = y + j;
        if (sy < 0 || sy >= m_h) continue;
        for (int i = 0; i < img.w; i++) {
            int sx = x + i;
            if (sx < 0 || sx >= m_w) continue;
            int v = img.px[j * img.w + i];
            float a = std::clamp((v - 32) / 192.0f, 0.0f, 1.0f) * strength;
            if (a <= 0) continue;
            uint32_t& d = m_px[sy * m_w + sx];
            auto mix = [a](int dst, int src) { return static_cast<uint8_t>(dst + (src - dst) * a); };
            Rgb o{mix((d >> 16) & 255, ink.r), mix((d >> 8) & 255, ink.g), mix(d & 255, ink.b)};
            d = pack(o);
        }
    }
}

void FrameBuffer::blitImg(const EgaImg& img, int y0, int y1, Rgb on, Rgb off, bool offTransparent) {
    y0 = std::max(0, y0);
    y1 = std::min({y1, EgaImg::H, m_h});
    int w = std::min(EgaImg::W, m_w);
    for (int y = y0; y < y1; y++)
        for (int x = 0; x < w; x++) {
            if (img.pixel(x, y)) m_px[y * m_w + x] = pack(on);
            else if (!offTransparent) m_px[y * m_w + x] = pack(off);
        }
}

void FrameBuffer::text(int col, int row, const char* s, Rgb fg, const Rgb* bg, int cellW, int cellH) {
    int x0 = col * cellW, y0 = row * cellH;
    for (; *s; s++, x0 += cellW) {
        const uint8_t* g = font8x8Glyph(*s);
        for (int y = 0; y < cellH; y++) {
            uint8_t bits = g[y * 8 / cellH]; // stretch 8 rows to the cell height
            for (int x = 0; x < cellW; x++) {
                bool on = x < 8 && (bits & (1 << x));
                if (on) put(x0 + x, y0 + y, fg);
                else if (bg) put(x0 + x, y0 + y, *bg);
            }
        }
    }
}

void FrameBuffer::textPx(int x, int y, const char* s, Rgb fg, int scale) {
    for (; *s; s++, x += 8 * scale) {
        const uint8_t* g = font8x8Glyph(*s);
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                if (g[r] & (1 << c)) fillRect(x + c * scale, y + r * scale, scale, scale, fg);
    }
}

bool FrameBuffer::save(const char* path) const {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, m_w, m_h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return false;
    int k = static_cast<int>(std::clamp(m_bright, 0.0f, 1.0f) * 256);
    for (int y = 0; y < m_h; y++) {
        uint32_t* dst = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(s->pixels) + y * s->pitch);
        for (int x = 0; x < m_w; x++) {
            uint32_t p = m_px[y * m_w + x];
            dst[x] = 0xFF000000u | ((((p >> 16) & 255) * k >> 8) << 16) |
                     ((((p >> 8) & 255) * k >> 8) << 8) | ((p & 255) * k >> 8);
        }
    }
    bool ok = SDL_SaveBMP(s, path) == 0;
    SDL_FreeSurface(s);
    return ok;
}

void FrameBuffer::present() {
    void* pixels;
    int pitch;
    if (SDL_LockTexture(m_tex, nullptr, &pixels, &pitch) == 0) {
        int k = static_cast<int>(std::clamp(m_bright, 0.0f, 1.0f) * 256);
        for (int y = 0; y < m_h; y++) {
            uint32_t* dst = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(pixels) + y * pitch);
            const uint32_t* src = &m_px[y * m_w];
            if (k >= 256) {
                std::copy(src, src + m_w, dst);
            } else {
                for (int x = 0; x < m_w; x++) {
                    uint32_t p = src[x];
                    dst[x] = 0xFF000000u | ((((p >> 16) & 255) * k >> 8) << 16) |
                             ((((p >> 8) & 255) * k >> 8) << 8) | ((p & 255) * k >> 8);
                }
            }
        }
        SDL_UnlockTexture(m_tex);
    }

    SDL_SetRenderTarget(m_sdl, nullptr);
    SDL_SetRenderDrawColor(m_sdl, 0, 0, 0, 255);
    SDL_RenderClear(m_sdl);
    int ww, wh;
    SDL_GetRendererOutputSize(m_sdl, &ww, &wh);
    int w = ww, h = ww * 3 / 4;
    if (h > wh) { h = wh; w = wh * 4 / 3; }
    SDL_Rect dst{(ww - w) / 2, (wh - h) / 2, w, h};
    SDL_RenderCopy(m_sdl, m_tex, nullptr, &dst);
    SDL_RenderPresent(m_sdl);
}

} // namespace corncob
