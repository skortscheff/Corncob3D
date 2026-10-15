// Renderer.cpp - Wireframe 3D renderer
// Projection follows f3dpt in f3dvec.asm (x' = x*d/z); near-plane clip replaces clip.asm.

#include "Renderer.hpp"
#include "Font8x8.hpp"
#include "../math/Trig.hpp"
#include <cmath>
#include <cstring>

namespace corncob {

static const SDL_Color EGA[16] = {
    {0x00, 0x00, 0x00, 255}, {0x00, 0x00, 0xAA, 255}, {0x00, 0xAA, 0x00, 255}, {0x00, 0xAA, 0xAA, 255},
    {0xAA, 0x00, 0x00, 255}, {0xAA, 0x00, 0xAA, 255}, {0xAA, 0x55, 0x00, 255}, {0xAA, 0xAA, 0xAA, 255},
    {0x55, 0x55, 0x55, 255}, {0x55, 0x55, 0xFF, 255}, {0x55, 0xFF, 0x55, 255}, {0x55, 0xFF, 0xFF, 255},
    {0xFF, 0x55, 0x55, 255}, {0xFF, 0x55, 0xFF, 255}, {0xFF, 0xFF, 0x55, 255}, {0xFF, 0xFF, 0xFF, 255},
};

static constexpr float NEAR_Z = 1.0f;
static const SDL_Rect GFX_RECT{Renderer::GFX_MIN_X, Renderer::GFX_MIN_Y,
                               Renderer::GFX_MAX_X - Renderer::GFX_MIN_X + 1,
                               Renderer::GFX_MAX_Y - Renderer::GFX_MIN_Y + 1};

Renderer::Renderer(SDL_Renderer* sdl) : m_sdl(sdl) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    m_target = SDL_CreateTexture(m_sdl, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WIDTH, HEIGHT);
}

Renderer::~Renderer() {
    if (m_panel) SDL_DestroyTexture(m_panel);
    if (m_target) SDL_DestroyTexture(m_target);
}

void Renderer::setColor(uint8_t c) {
    const SDL_Color& col = EGA[c & 15];
    SDL_SetRenderDrawColor(m_sdl, col.r, col.g, col.b, 255);
}

void Renderer::setPanel(const EgaImg& img) {
    if (m_panel) SDL_DestroyTexture(m_panel);
    const int y0 = GFX_MAX_Y + 1, h = HEIGHT - y0;
    m_panel = SDL_CreateTexture(m_sdl, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, WIDTH, h);
    if (!m_panel) return;
    std::vector<uint32_t> px(static_cast<size_t>(WIDTH) * h);
    const SDL_Color& on = EGA[LIGHTGRAY]; // rimg draws set bits in color n = 7
    for (int y = 0; y < h; y++)
        for (int x = 0; x < WIDTH; x++)
            px[y * WIDTH + x] = img.pixel(x, y0 + y)
                ? 0xFF000000u | (on.r << 16) | (on.g << 8) | on.b
                : 0xFF000000u;
    SDL_UpdateTexture(m_panel, nullptr, px.data(), WIDTH * 4);
}

void Renderer::beginFrame(uint8_t clearColor) {
    SDL_SetRenderTarget(m_sdl, m_target);
    SDL_RenderSetClipRect(m_sdl, nullptr);
    SDL_SetRenderDrawColor(m_sdl, 0, 0, 0, 255);
    SDL_RenderClear(m_sdl);
    SDL_RenderSetClipRect(m_sdl, &GFX_RECT);
    setColor(clearColor);
    SDL_RenderFillRect(m_sdl, &GFX_RECT);
}

void Renderer::drawPanel() {
    SDL_RenderSetClipRect(m_sdl, nullptr);
    if (m_panel) {
        SDL_Rect dst{0, GFX_MAX_Y + 1, WIDTH, HEIGHT - (GFX_MAX_Y + 1)};
        SDL_RenderCopy(m_sdl, m_panel, nullptr, &dst);
    }
}

void Renderer::endFrame() {
    SDL_RenderSetClipRect(m_sdl, nullptr);
    SDL_SetRenderTarget(m_sdl, nullptr);
    SDL_SetRenderDrawColor(m_sdl, 0, 0, 0, 255);
    SDL_RenderClear(m_sdl);

    // Letterbox to 4:3 inside whatever size the window is
    int ww, wh;
    SDL_GetRendererOutputSize(m_sdl, &ww, &wh);
    int w = ww, h = ww * 3 / 4;
    if (h > wh) { h = wh; w = wh * 4 / 3; }
    SDL_Rect dst{(ww - w) / 2, (wh - h) / 2, w, h};
    SDL_RenderCopy(m_sdl, m_target, nullptr, &dst);
    SDL_RenderPresent(m_sdl);
}

bool Renderer::saveScreenshot(const char* path) {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, WIDTH, HEIGHT, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return false;
    bool ok = SDL_RenderReadPixels(m_sdl, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch) == 0
              && SDL_SaveBMP(s, path) == 0;
    SDL_FreeSurface(s);
    return ok;
}

void Renderer::setCamera(const P3& pos, const Mat3x3& orient) {
    m_camPos = pos;
    m_camOrient = orient;
}

bool Renderer::toView(const P3& w, P3& v) const {
    float rel[3] = {w.x - m_camPos.x, w.y - m_camPos.y, w.z - m_camPos.z};
    float out[3];
    m_camOrient.transform(rel, out);
    v = {out[0], out[1], out[2]};
    return v.z >= NEAR_Z;
}

// f3dpt: screen = center + coord * plane / z, with separate x/y planes for EGA's tall pixels
P3 Renderer::project(const P3& v) const {
    return {GFX_CTR_X + v.x * GFX_PLN_X / v.z,
            GFX_CTR_Y - v.y * GFX_PLN_Y / v.z,
            v.z};
}

void Renderer::line3D(const P3& a, const P3& b, uint8_t color) {
    P3 va, vb;
    bool ina = toView(a, va), inb = toView(b, vb);
    if (!ina && !inb) return;
    // Clip against the near plane (z = NEAR_Z)
    if (!ina || !inb) {
        float t = (NEAR_Z - va.z) / (vb.z - va.z);
        P3 c{va.x + (vb.x - va.x) * t, va.y + (vb.y - va.y) * t, NEAR_Z};
        if (!ina) va = c; else vb = c;
    }
    P3 pa = project(va), pb = project(vb);
    // Reject absurdly long lines before handing to SDL (int overflow guard)
    const float LIM = 32000.0f;
    if (std::fabs(pa.x) > LIM || std::fabs(pa.y) > LIM || std::fabs(pb.x) > LIM || std::fabs(pb.y) > LIM) {
        // Pull the far point toward the near one until it fits
        for (int i = 0; i < 16 && (std::fabs(pb.x) > LIM || std::fabs(pb.y) > LIM); i++) {
            pb.x = (pa.x + pb.x) / 2; pb.y = (pa.y + pb.y) / 2;
        }
        for (int i = 0; i < 16 && (std::fabs(pa.x) > LIM || std::fabs(pa.y) > LIM); i++) {
            pa.x = (pa.x + pb.x) / 2; pa.y = (pa.y + pb.y) / 2;
        }
    }
    setColor(color);
    SDL_RenderDrawLineF(m_sdl, pa.x, pa.y, pb.x, pb.y);
}

void Renderer::model(const Model& m, const P3& pos, float yawDeg) {
    float c = std::cos(yawDeg * DEG2RAD), s = std::sin(yawDeg * DEG2RAD);
    std::vector<P3> world(m.verts.size());
    for (size_t i = 0; i < m.verts.size(); i++) {
        const P3& v = m.verts[i];
        world[i] = {pos.x + v.x * c + v.z * s, pos.y + v.y, pos.z - v.x * s + v.z * c};
    }
    for (auto& e : m.edges) line3D(world[e.first], world[e.second], m.color);
}

void Renderer::model(const Model& m, const P3& pos, const Mat3x3& o, float scale) {
    // world = pos + scale * (R^T * local), rows of R are the body axes
    auto xf = [&](const P3& v) {
        return P3{pos.x + scale * (o.m[0] * v.x + o.m[3] * v.y + o.m[6] * v.z),
                  pos.y + scale * (o.m[1] * v.x + o.m[4] * v.y + o.m[7] * v.z),
                  pos.z + scale * (o.m[2] * v.x + o.m[5] * v.y + o.m[8] * v.z)};
    };
    for (auto& e : m.edges) line3D(xf(m.verts[e.first]), xf(m.verts[e.second]), m.color);
    for (auto& pl : m.polys) {
        size_t n = pl.pts.size();
        if (n < 2) continue;
        P3 first = xf(pl.pts[0]), prev = first;
        for (size_t i = 1; i < n; i++) {
            P3 cur = xf(pl.pts[i]);
            line3D(prev, cur, pl.color);
            prev = cur;
        }
        if (pl.closed && n > 2) line3D(prev, first, pl.color);
    }
    for (auto& s : m.spheres) sphere3D(xf(s.c), s.r * scale, s.color);
}

void Renderer::sphere3D(const P3& c, float r, uint8_t color) {
    P3 v;
    if (!toView(c, v) || v.z < r) return;
    P3 sc = project(v);
    float rx = r * GFX_PLN_X / v.z;
    float ry = r * GFX_PLN_Y / v.z;
    setColor(color);
    if (rx < 0.5f) { SDL_RenderDrawPointF(m_sdl, sc.x, sc.y); return; }
    if (rx > 4000.0f) return;
    const int segs = rx < 8 ? 12 : 32;
    float px = sc.x + rx, py = sc.y;
    for (int i = 1; i <= segs; i++) {
        float a = i * 2.0f * PI / segs;
        float nx = sc.x + rx * std::cos(a), ny = sc.y + ry * std::sin(a);
        SDL_RenderDrawLineF(m_sdl, px, py, nx, ny);
        px = nx; py = ny;
    }
}

void Renderer::line2D(int x0, int y0, int x1, int y1, uint8_t color) {
    setColor(color);
    SDL_RenderDrawLine(m_sdl, x0, y0, x1, y1);
}

void Renderer::rect2D(int x, int y, int w, int h, uint8_t color, bool filled) {
    setColor(color);
    SDL_Rect r{x, y, w, h};
    if (filled) SDL_RenderFillRect(m_sdl, &r); else SDL_RenderDrawRect(m_sdl, &r);
}

void Renderer::horizon(uint8_t sky, uint8_t ground) {
    // Direction to the horizon is any horizontal world vector; its view-space image
    // gives a line on screen. Use the camera's forward projected onto the ground plane.
    const float* f = &m_camOrient.m[6];
    float hx = f[0], hz = f[2];
    float hl = std::sqrt(hx * hx + hz * hz);
    if (hl < 1e-4f) { hx = -m_camOrient.m[3]; hz = -m_camOrient.m[5]; hl = std::sqrt(hx * hx + hz * hz); }
    hx /= hl; hz /= hl;
    // Perpendicular horizontal direction
    float px = hz, pz = -hx;

    // View-space vectors (directions, no translation)
    auto dirView = [&](float x, float y, float z) {
        float in[3] = {x, y, z}, out[3];
        m_camOrient.transform(in, out);
        return P3{out[0], out[1], out[2]};
    };
    P3 d = dirView(hx, 0, hz);
    P3 p = dirView(px, 0, pz);
    P3 up = dirView(0, 1, 0);

    if (d.z <= 1e-3f) {
        // Looking straight up or down: whole window is sky or ground
        setColor(up.z > 0 ? sky : ground);
        SDL_RenderFillRect(m_sdl, &GFX_RECT);
        return;
    }
    setColor(sky);
    SDL_RenderFillRect(m_sdl, &GFX_RECT);

    // Screen point of the horizon direction, and the line's slope from the perpendicular
    float cx = GFX_CTR_X + d.x * GFX_PLN_X / d.z;
    float cy = GFX_CTR_Y - d.y * GFX_PLN_Y / d.z;
    float sx = p.x * GFX_PLN_X, sy = -p.y * GFX_PLN_Y;
    if (std::fabs(sx) < 1e-6f) sx = 1e-6f;
    float slope = sy / sx;

    // Ground lies on the side of the line opposite to screen-space "up"
    float upSx = up.x * GFX_PLN_X, upSy = -up.y * GFX_PLN_Y;
    bool groundBelow = (-upSy) - slope * (-upSx) > 0;

    // Fill ground pixel columns (one vertical line per column)
    setColor(ground);
    for (int x = GFX_MIN_X; x <= GFX_MAX_X; x++) {
        float yh = cy + slope * (x - cx);
        if (groundBelow) {
            int y0 = static_cast<int>(std::ceil(yh));
            if (y0 <= GFX_MAX_Y) SDL_RenderDrawLine(m_sdl, x, y0 < GFX_MIN_Y ? GFX_MIN_Y : y0, x, GFX_MAX_Y);
        } else {
            int y1 = static_cast<int>(std::floor(yh));
            if (y1 >= GFX_MIN_Y) SDL_RenderDrawLine(m_sdl, x, GFX_MIN_Y, x, y1 > GFX_MAX_Y ? GFX_MAX_Y : y1);
        }
    }
}

void Renderer::text(int x, int y, const char* s, uint8_t color, int scale) {
    setColor(color);
    for (; *s; s++, x += 8 * scale) {
        const uint8_t* g = font8x8Glyph(*s);
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                if (g[r] & (1 << c)) {
                    SDL_Rect px{x + c * scale, y + r * scale, scale, scale};
                    SDL_RenderFillRect(m_sdl, &px);
                }
    }
}

} // namespace corncob
