// EgaScreen.cpp - Indexed EGA pages, palette and presentation

#include "EgaScreen.hpp"
#include "Font8x8.hpp"
#include <algorithm>

namespace corncob {

void EgaScreen::setScale(int s) {
    m_scale = std::max(1, s);
    m_w = BASE_W * m_scale;
    m_h = BASE_H * m_scale;
    for (auto& p : m_pages) p.assign(static_cast<size_t>(m_w) * m_h, 0);
    m_back = 0;
}

void EgaScreen::hspan(int x0, int x1, int y, uint8_t c) {
    if (y < 0 || y >= m_h) return;
    x0 = std::max(x0, 0);
    x1 = std::min(x1, m_w);
    if (x0 >= x1) return;
    uint8_t* row = back() + static_cast<size_t>(y) * m_w;
    if (m_mask == 15) {
        std::fill(row + x0, row + x1, c);
    } else {
        for (int x = x0; x < x1; x++) row[x] = static_cast<uint8_t>((row[x] & ~m_mask) | (c & m_mask));
    }
}

void EgaScreen::fillRectBoth(int x, int y, int w, int h, uint8_t c) {
    for (auto& page : m_pages)
        for (int j = std::max(y, 0); j < std::min(y + h, m_h); j++)
            for (int i = std::max(x, 0); i < std::min(x + w, m_w); i++) page[static_cast<size_t>(j) * m_w + i] = c;
}

void EgaScreen::replaceColor(int x0, int y0, int x1, int y1, uint8_t from, uint8_t to) {
    const int s = m_scale;
    for (auto& page : m_pages)
        for (int y = std::max(y0 * s, 0); y < std::min((y1 + 1) * s, m_h); y++)
            for (int x = std::max(x0 * s, 0); x < std::min((x1 + 1) * s, m_w); x++) {
                uint8_t& p = page[static_cast<size_t>(y) * m_w + x];
                if ((p & 15) == from) p = to;
            }
}

void EgaScreen::fillRect(int x, int y, int w, int h, uint8_t c) {
    for (int j = y; j < y + h; j++) hspan(x, x + w, j, c);
}

void EgaScreen::drawPanel(const EgaImg& img, int fromRow) {
    for (auto& page : m_pages)
        for (int y = fromRow * m_scale; y < m_h; y++)
            for (int x = 0; x < m_w; x++)
                page[static_cast<size_t>(y) * m_w + x] = img.pixel(x / m_scale, y / m_scale) ? LIGHTGRAY : BLACK;
}

void EgaScreen::text(int x, int y, const char* s, uint8_t color) {
    for (; *s; s++, x += 8) {
        const uint8_t* g = font8x8Glyph(*s);
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                if (g[r] & (1 << c)) fillRect((x + c) * m_scale, (y + r) * m_scale, m_scale, m_scale, color);
    }
}

EgaPalette EgaPalette::defaults() {
    EgaPalette p;
    // BIOS mode 10h attribute registers
    static const uint8_t attr[16] = {0, 1, 2, 3, 4, 5, 0x14, 7, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F};
    std::copy(attr, attr + 16, p.attr.begin());
    // VGA default DAC for the 64 EGA colors: bits rgbRGB, primary 42 + secondary 21
    for (int i = 0; i < 64; i++) {
        p.dac[i][0] = static_cast<uint8_t>(((i >> 2) & 1) * 42 + ((i >> 5) & 1) * 21);
        p.dac[i][1] = static_cast<uint8_t>(((i >> 1) & 1) * 42 + ((i >> 4) & 1) * 21);
        p.dac[i][2] = static_cast<uint8_t>((i & 1) * 42 + ((i >> 3) & 1) * 21);
    }
    return p;
}

Rgb EgaPalette::rgb(uint8_t color) const {
    const auto& d = dac[attr[color & 15] & 63];
    auto to8 = [](uint8_t v) { return static_cast<uint8_t>((v & 63) * 255 / 63); };
    return {to8(d[0]), to8(d[1]), to8(d[2])};
}

Display::~Display() {
    if (m_tex) SDL_DestroyTexture(m_tex);
}

void Display::convert(const EgaScreen& scr, const EgaPalette& pal, int panX, int panY) {
    uint32_t lut[16];
    for (int i = 0; i < 16; i++) {
        Rgb c = pal.rgb(static_cast<uint8_t>(i));
        lut[i] = 0xFF000000u | (c.r << 16) | (c.g << 8) | c.b;
    }
    const int w = scr.width(), h = scr.height();
    m_px.resize(static_cast<size_t>(w) * h);
    const uint8_t* src = scr.front();
    // A start-address offset scrolls the whole display; what comes into view
    // past the end of the page is shown black
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int sx = x + panX, sy = y + panY;
            m_px[static_cast<size_t>(y) * w + x] =
                (sx >= 0 && sx < w && sy >= 0 && sy < h) ? lut[src[static_cast<size_t>(sy) * w + sx] & 15] : 0xFF000000u;
        }
}

void Display::present(const EgaScreen& scr, const EgaPalette& pal, int panX, int panY) {
    const bool filter = m_smooth && scr.scale() > 1;
    if (!m_tex || m_texW != scr.width() || m_texH != scr.height() || m_texSmooth != filter) {
        if (m_tex) SDL_DestroyTexture(m_tex);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, filter ? "1" : "0");   // read when the texture is made
        m_texSmooth = filter;
        m_texW = scr.width();
        m_texH = scr.height();
        m_tex = SDL_CreateTexture(m_sdl, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, m_texW, m_texH);
    }
    convert(scr, pal, panX, panY);
    SDL_UpdateTexture(m_tex, nullptr, m_px.data(), m_texW * 4);

    SDL_SetRenderTarget(m_sdl, nullptr);
    SDL_SetRenderDrawColor(m_sdl, 0, 0, 0, 255);
    SDL_RenderClear(m_sdl);
    int ww, wh;
    SDL_GetRendererOutputSize(m_sdl, &ww, &wh);
    int w = ww, h = ww * 3 / 4;
    if (h > wh) { h = wh; w = wh * 4 / 3; }
    SDL_Rect dst{(ww - w) / 2, (wh - h) / 2, w, h};
    SDL_RenderCopy(m_sdl, m_tex, nullptr, &dst);
    if (m_overlay) m_overlay(dst);
    SDL_RenderPresent(m_sdl);
}

bool Display::saveBmp(const EgaScreen& scr, const EgaPalette& pal, const char* path) const {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, scr.width(), scr.height(), 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return false;
    const uint8_t* src = scr.front();
    for (int y = 0; y < scr.height(); y++) {
        uint32_t* dst = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(s->pixels) + y * s->pitch);
        for (int x = 0; x < scr.width(); x++) {
            Rgb c = pal.rgb(src[static_cast<size_t>(y) * scr.width() + x]);
            dst[x] = 0xFF000000u | (c.r << 16) | (c.g << 8) | c.b;
        }
    }
    bool ok = SDL_SaveBMP(s, path) == 0;
    SDL_FreeSurface(s);
    return ok;
}

} // namespace corncob
