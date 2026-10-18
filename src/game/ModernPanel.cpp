// ModernPanel.cpp - The smooth instrument panel (see ModernPanel.hpp)
//
// Everything is drawn with distance-field shapes (one coverage value per pixel), so edges are anti-aliased at any
// window size. The static part (bezels, dial faces, scales, lamp outlines) is painted once per window size; each
// frame it is copied and the hands, the horizon, the lamps and the bars are painted on top.
//
// Angles of the dials are the ones of INST.ASM (16-bit binary angles, 65536 = a turn, 0 = up, clockwise), so a
// hand points where the original needle points.

#include "ModernPanel.hpp"
#include "DGroup.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace corncob::game {

namespace d = corncob::dgroup;
namespace lbl = corncob::lbl;

namespace {

constexpr float PI = 3.14159265f;
constexpr float TAU = 6.2831853f;

struct Col {
    float r, g, b;
};
constexpr Col rgb(uint32_t v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
inline Col mix(Col a, Col b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
inline Col scaled(Col a, float k) { return {a.r * k, a.g * k, a.b * k}; }
inline float sat(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// the palette of the panel
const Col C_PANEL_TOP = rgb(0x2b2f36), C_PANEL_BOT = rgb(0x181a1f);
const Col C_BEZEL_LT = rgb(0xc4cad3), C_BEZEL_DK = rgb(0x3b4048);
const Col C_FACE_IN = rgb(0x1b1f26), C_FACE_OUT = rgb(0x0b0c0f);
const Col C_TICK = rgb(0xe9edf2), C_DIM = rgb(0x8d95a1), C_TEXT = rgb(0xd7dce3);
const Col C_HAND = rgb(0xf4f6f9), C_ORANGE = rgb(0xff8c1a), C_CYAN = rgb(0x4cc9f0);
const Col C_GREEN = rgb(0x3ddc84), C_AMBER = rgb(0xffb020), C_RED = rgb(0xff4538), C_BLUE = rgb(0x3d8bff);
const Col C_LAMP_OFF = rgb(0x22252b), C_LAMP_EDGE = rgb(0x3a3f47), C_LAMP_TXT = rgb(0x646b76);
const Col C_SKY_HI = rgb(0x2f6fb8), C_SKY_LO = rgb(0x7bbcf0), C_GND_HI = rgb(0xb18a52), C_GND_LO = rgb(0x6b4c27);

// ---------------------------------------------------------------------------------------------
// The canvas: distance-field shapes with one coverage value per pixel
// ---------------------------------------------------------------------------------------------
struct Cv {
    uint32_t* p;
    int w, h;

    void put(int x, int y, Col c, float a) {
        if (a <= 0.f || x < 0 || y < 0 || x >= w || y >= h) return;
        uint32_t& dst = p[static_cast<size_t>(y) * w + x];
        const float dr = static_cast<float>((dst >> 16) & 255), dg = static_cast<float>((dst >> 8) & 255), db = static_cast<float>(dst & 255);
        const int r = static_cast<int>(dr + (c.r * 255.f - dr) * a + 0.5f);
        const int g = static_cast<int>(dg + (c.g * 255.f - dg) * a + 0.5f);
        const int b = static_cast<int>(db + (c.b * 255.f - db) * a + 0.5f);
        dst = 0xFF000000u | (static_cast<uint32_t>(std::clamp(r, 0, 255)) << 16) | (static_cast<uint32_t>(std::clamp(g, 0, 255)) << 8) |
              static_cast<uint32_t>(std::clamp(b, 0, 255));
    }

    // sd(x, y): signed distance to the shape in pixels (negative inside); cf(x, y): the color
    template <class SD, class CF>
    void shape(float x0, float y0, float x1, float y1, SD sd, CF cf, float alpha = 1.f) {
        const int ix0 = std::max(0, static_cast<int>(std::floor(x0))), iy0 = std::max(0, static_cast<int>(std::floor(y0)));
        const int ix1 = std::min(w - 1, static_cast<int>(std::ceil(x1))), iy1 = std::min(h - 1, static_cast<int>(std::ceil(y1)));
        for (int y = iy0; y <= iy1; y++)
            for (int x = ix0; x <= ix1; x++) {
                const float px = x + 0.5f, py = y + 0.5f;
                const float a = 0.5f - sd(px, py);
                if (a <= 0.f) continue;
                put(x, y, cf(px, py), (a > 1.f ? 1.f : a) * alpha);
            }
    }

    void fill(Col top, Col bot) {
        for (int y = 0; y < h; y++) {
            const Col c = mix(top, bot, static_cast<float>(y) / static_cast<float>(std::max(1, h - 1)));
            const uint32_t v = 0xFF000000u | (static_cast<uint32_t>(c.r * 255.f) << 16) | (static_cast<uint32_t>(c.g * 255.f) << 8) |
                               static_cast<uint32_t>(c.b * 255.f);
            for (int x = 0; x < w; x++) p[static_cast<size_t>(y) * w + x] = v;
        }
    }

    template <class CF>
    void disc(float cx, float cy, float r, CF cf, float alpha = 1.f) {
        shape(cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1, [&](float x, float y) { return std::hypot(x - cx, y - cy) - r; }, cf, alpha);
    }
    void disc(float cx, float cy, float r, Col c, float alpha = 1.f) {
        disc(cx, cy, r, [&](float, float) { return c; }, alpha);
    }
    template <class CF>
    void ring(float cx, float cy, float r, float t, CF cf, float alpha = 1.f) {
        shape(cx - r - t, cy - r - t, cx + r + t, cy + r + t,
              [&](float x, float y) { return std::fabs(std::hypot(x - cx, y - cy) - r) - t * 0.5f; }, cf, alpha);
    }
    void ring(float cx, float cy, float r, float t, Col c, float alpha = 1.f) {
        ring(cx, cy, r, t, [&](float, float) { return c; }, alpha);
    }

    void line(float x0, float y0, float x1, float y1, float wd, Col c, float alpha = 1.f) {
        const float dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
        shape(std::min(x0, x1) - wd, std::min(y0, y1) - wd, std::max(x0, x1) + wd, std::max(y0, y1) + wd,
              [&](float x, float y) {
                  float t = len2 > 0 ? ((x - x0) * dx + (y - y0) * dy) / len2 : 0.f;
                  t = sat(t);
                  return std::hypot(x - (x0 + dx * t), y - (y0 + dy * t)) - wd * 0.5f;
              },
              [&](float, float) { return c; }, alpha);
    }

    // an arc of the circle (angles clockwise from up, radians), round ends
    void arc(float cx, float cy, float r, float a0, float a1, float wd, Col c, float alpha = 1.f) {
        const float ex0 = cx + r * std::sin(a0), ey0 = cy - r * std::cos(a0), ex1 = cx + r * std::sin(a1), ey1 = cy - r * std::cos(a1);
        shape(cx - r - wd, cy - r - wd, cx + r + wd, cy + r + wd,
              [&](float x, float y) {
                  const float dx = x - cx, dy = y - cy;
                  float t = std::fmod(std::atan2(dx, -dy) - a0, TAU);
                  if (t < 0) t += TAU;
                  if (t <= a1 - a0) return std::fabs(std::hypot(dx, dy) - r) - wd * 0.5f;
                  return std::min(std::hypot(x - ex0, y - ey0), std::hypot(x - ex1, y - ey1)) - wd * 0.5f;
              },
              [&](float, float) { return c; }, alpha);
    }

    template <class CF>
    void rrect(float x0, float y0, float x1, float y1, float rad, CF cf, float alpha = 1.f) {
        const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f, hx = (x1 - x0) * 0.5f - rad, hy = (y1 - y0) * 0.5f - rad;
        shape(x0 - 1, y0 - 1, x1 + 1, y1 + 1,
              [&](float x, float y) {
                  const float qx = std::fabs(x - cx) - hx, qy = std::fabs(y - cy) - hy;
                  return std::hypot(std::max(qx, 0.f), std::max(qy, 0.f)) + std::min(std::max(qx, qy), 0.f) - rad;
              },
              cf, alpha);
    }
    void rrect(float x0, float y0, float x1, float y1, float rad, Col c, float alpha = 1.f) {
        rrect(x0, y0, x1, y1, rad, [&](float, float) { return c; }, alpha);
    }
    // the outline of a rounded rectangle
    void rrectLine(float x0, float y0, float x1, float y1, float rad, float t, Col c, float alpha = 1.f) {
        const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f, hx = (x1 - x0) * 0.5f - rad, hy = (y1 - y0) * 0.5f - rad;
        shape(x0 - t, y0 - t, x1 + t, y1 + t,
              [&](float x, float y) {
                  const float qx = std::fabs(x - cx) - hx, qy = std::fabs(y - cy) - hy;
                  const float sd = std::hypot(std::max(qx, 0.f), std::max(qy, 0.f)) + std::min(std::max(qx, qy), 0.f) - rad;
                  return std::fabs(sd) - t * 0.5f;
              },
              [&](float, float) { return c; }, alpha);
    }

    // a convex polygon (points in any order around it)
    void poly(const float* xy, int n, Col c, float alpha = 1.f) {
        float mx = 0, my = 0, x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (int i = 0; i < n; i++) {
            mx += xy[2 * i];
            my += xy[2 * i + 1];
            x0 = std::min(x0, xy[2 * i]);
            x1 = std::max(x1, xy[2 * i]);
            y0 = std::min(y0, xy[2 * i + 1]);
            y1 = std::max(y1, xy[2 * i + 1]);
        }
        mx /= n;
        my /= n;
        float nx[8], ny[8], off[8];
        for (int i = 0; i < n; i++) {
            const float ax = xy[2 * i], ay = xy[2 * i + 1], bx = xy[2 * ((i + 1) % n)], by = xy[2 * ((i + 1) % n) + 1];
            float ex = by - ay, ey = ax - bx;   // a normal of the edge
            const float l = std::hypot(ex, ey);
            ex /= l;
            ey /= l;
            if (ex * (mx - ax) + ey * (my - ay) > 0) { ex = -ex; ey = -ey; }   // point it outwards
            nx[i] = ex;
            ny[i] = ey;
            off[i] = ex * ax + ey * ay;
        }
        shape(x0 - 1, y0 - 1, x1 + 1, y1 + 1,
              [&](float x, float y) {
                  float m = -1e9f;
                  for (int i = 0; i < n; i++) m = std::max(m, nx[i] * x + ny[i] * y - off[i]);
                  return m;
              },
              [&](float, float) { return c; }, alpha);
    }
};

// ---------------------------------------------------------------------------------------------
// A stroke font (digits and capitals), 4 wide and 6 high, strokes separated by ';'
// ---------------------------------------------------------------------------------------------
struct Glyph {
    char c;
    const char* s;
};
const Glyph GLYPHS[] = {
    {'0', "1,0 3,0 4,1 4,5 3,6 1,6 0,5 0,1 1,0"},
    {'1', "1,1 2,0 2,6;1,6 3,6"},
    {'2', "0,1 1,0 3,0 4,1 4,2 0,6 4,6"},
    {'3', "0,0 4,0 2,2.6 3,3 4,4 4,5 3,6 1,6 0,5"},
    {'4', "3,6 3,0 0,4 4,4"},
    {'5', "4,0 0,0 0,3 3,3 4,4 4,5 3,6 1,6 0,5"},
    {'6', "4,1 3,0 1,0 0,1 0,5 1,6 3,6 4,5 4,4 3,3 0,3"},
    {'7', "0,0 4,0 1.5,6"},
    {'8', "1,0 3,0 4,1 4,2 3,3 1,3 0,2 0,1 1,0;1,3 0,4 0,5 1,6 3,6 4,5 4,4 3,3"},
    {'9', "0,5 1,6 3,6 4,5 4,1 3,0 1,0 0,1 0,2 1,3 4,3"},
    {'A', "0,6 2,0 4,6;0.9,4 3.1,4"},
    {'B', "0,6 0,0 3,0 4,1 4,2 3,3 0,3;3,3 4,4 4,5 3,6 0,6"},
    {'C', "4,1 3,0 1,0 0,1 0,5 1,6 3,6 4,5"},
    {'D', "0,0 0,6 2,6 4,4 4,2 2,0 0,0"},
    {'E', "4,0 0,0 0,6 4,6;0,3 3,3"},
    {'F', "4,0 0,0 0,6;0,3 3,3"},
    {'G', "4,1 3,0 1,0 0,1 0,5 1,6 3,6 4,5 4,3 2,3"},
    {'H', "0,0 0,6;4,0 4,6;0,3 4,3"},
    {'I', "1,0 3,0;2,0 2,6;1,6 3,6"},
    {'J', "4,0 4,5 3,6 1,6 0,5"},
    {'K', "0,0 0,6;4,0 0,3.5;1.5,2.5 4,6"},
    {'L', "0,0 0,6 4,6"},
    {'M', "0,6 0,0 2,3 4,0 4,6"},
    {'N', "0,6 0,0 4,6 4,0"},
    {'O', "1,0 3,0 4,1 4,5 3,6 1,6 0,5 0,1 1,0"},
    {'P', "0,6 0,0 3,0 4,1 4,2 3,3 0,3"},
    {'Q', "1,0 3,0 4,1 4,5 3,6 1,6 0,5 0,1 1,0;2.5,4 4,6"},
    {'R', "0,6 0,0 3,0 4,1 4,2 3,3 0,3;2,3 4,6"},
    {'S', "4,1 3,0 1,0 0,1 0,2 1,3 3,3 4,4 4,5 3,6 1,6 0,5"},
    {'T', "0,0 4,0;2,0 2,6"},
    {'U', "0,0 0,5 1,6 3,6 4,5 4,0"},
    {'V', "0,0 2,6 4,0"},
    {'W', "0,0 1,6 2,3 3,6 4,0"},
    {'X', "0,0 4,6;4,0 0,6"},
    {'Y', "0,0 2,3 4,0;2,3 2,6"},
    {'Z', "0,0 4,0 0,6 4,6"},
    {'-', "0.5,3 3.5,3"},
    {'+', "0.5,3 3.5,3;2,1 2,5"},
    {'.', "2,5.7 2,6"},
    {'/', "0,6 4,0"},
    {'%', "0,6 4,0;0.5,0.5 1.3,0.5 1.3,1.5 0.5,1.5 0.5,0.5;2.7,4.5 3.5,4.5 3.5,5.5 2.7,5.5 2.7,4.5"},
    {'^', "0.7,0 2.1,0 2.1,1.4 0.7,1.4 0.7,0"},   // the degree sign
};

const char* glyphOf(char c) {
    for (const Glyph& g : GLYPHS)
        if (g.c == c) return g.s;
    return nullptr;
}

float textWidth(const char* s, float h) {
    const float u = h / 6.f;
    return std::strlen(s) * 6.f * u - 2.f * u;
}

// align: 0 centered on x, -1 starts at x, 1 ends at x; y is the middle of the line
void text(Cv& cv, const char* s, float x, float y, float h, Col c, int align = 0, float alpha = 1.f) {
    const float u = h / 6.f, sw = std::max(1.1f, h * 0.15f);
    float x0 = x;
    const float wd = textWidth(s, h);
    if (align == 0) x0 = x - wd * 0.5f;
    else if (align > 0) x0 = x - wd;
    const float y0 = y - h * 0.5f;
    for (; *s; s++, x0 += 6.f * u) {
        const char* q = glyphOf(static_cast<char>(std::toupper(static_cast<unsigned char>(*s))));
        if (!q) continue;
        while (*q) {   // a stroke: "x,y x,y ..." up to ';'
            float px = 0, py = 0;
            bool have = false;
            while (*q && *q != ';') {
                char* e = nullptr;
                const float gx = std::strtof(q, &e);
                if (e == q || *e != ',') { q = e; break; }
                const float gy = std::strtof(e + 1, &e);
                const float nx = x0 + gx * u, ny = y0 + gy * u;
                if (have) cv.line(px, py, nx, ny, sw, c, alpha);
                px = nx;
                py = ny;
                have = true;
                q = e;
                while (*q == ' ') q++;
            }
            if (*q == ';') q++;
            else if (*q) break;
        }
    }
}
inline void polar(float cx, float cy, float r, float a, float& x, float& y) {
    x = cx + r * std::sin(a);
    y = cy - r * std::cos(a);
}

// a hand: a thin kite from a tail behind the pivot to a tip
void hand(Cv& cv, float cx, float cy, float ang, float len, float tail, float wd, Col c, bool shadow = true) {
    const float s = std::sin(ang), co = std::cos(ang);
    auto pt = [&](float along, float side, float& x, float& y) {
        x = cx + s * along + co * side;
        y = cy - co * along + s * side;
    };
    float p[8];
    pt(len, 0, p[0], p[1]);
    pt(len * 0.12f, wd * 0.5f, p[2], p[3]);
    pt(-tail, 0, p[4], p[5]);
    pt(len * 0.12f, -wd * 0.5f, p[6], p[7]);
    if (shadow) {
        float q[8];
        for (int i = 0; i < 4; i++) { q[2 * i] = p[2 * i] + wd * 0.35f; q[2 * i + 1] = p[2 * i + 1] + wd * 0.5f; }
        cv.poly(q, 4, Col{0, 0, 0}, 0.35f);
    }
    cv.poly(p, 4, c);
}

float turn16(double frac) { return static_cast<float>((frac - std::floor(frac)) * TAU); }

struct Box {
    float x0, y0, x1, y1;   // 640x350 coordinates
};
constexpr Box BOX_TEMP{10, 208, 52, 240}, BOX_OIL{58, 208, 100, 240}, BOX_CMP{105, 208, 149, 240}, BOX_PIT{151, 208, 200, 240};
constexpr Box BOX_ALT{10, 243, 77, 291}, BOX_SPD{82, 243, 150, 291}, BOX_CLM{10, 293, 77, 342}, BOX_ATT{82, 293, 150, 342};
constexpr Box BOX_L1{202, 208, 251, 242}, BOX_ALW{253, 208, 352, 242}, BOX_CTL{355, 208, 436, 242}, BOX_L2{439, 208, 488, 242};
constexpr Box BOX_BMB{491, 208, 521, 242}, BOX_DMG{524, 208, 628, 242}, BOX_THR{491, 244, 628, 252};
constexpr Box REAR_FRAME{160, 248, 480, 349}, REAR_HOLE{168, 254, 472, 345};
constexpr Box RAD_FRAME{491, 254, 626, 342}, RAD_HOLE{496, 259, 616, 328};

// the buttons of the original panel (autodata), where the lamps are read back from
constexpr Box BTN[14] = {{205, 209, 248, 218}, {205, 219, 248, 228}, {205, 229, 248, 241}, {443, 209, 484, 219}, {443, 220, 484, 230},
                         {443, 231, 484, 241}, {492, 209, 517, 218}, {492, 219, 517, 228}, {492, 229, 517, 238}, {279, 216, 324, 229},
                         {52, 251, 61, 262},   {59, 256, 68, 267},   {267, 231, 336, 241}, {524, 208, 629, 238}};
constexpr int DMG_X = 536, DMG_Y[4] = {213, 219, 226, 233};

} // namespace

ModernPanel::~ModernPanel() {
    if (m_tex) SDL_DestroyTexture(m_tex);
}

bool ModernPanel::active() const {
    return m_g.w(d::m3::ptrgwinf) == static_cast<int16_t>(d::m3::gwinf);
}

void ModernPanel::ensure(int w, int h) {
    if (w == m_w && h == m_h && !m_static.px.empty()) return;
    m_w = w;
    m_h = h;
    m_sx = static_cast<float>(w) / 640.f;
    m_sy = static_cast<float>(h) / 350.f;
    m_top = static_cast<int>(std::lround(200 * m_sy));
    m_static.w = m_frame.w = w;
    m_static.h = m_frame.h = h - m_top;
    m_static.px.assign(static_cast<size_t>(m_static.w) * m_static.h, 0xFF000000u);
    m_frame.px = m_static.px;
    buildStatic();
    if (m_tex) {
        SDL_DestroyTexture(m_tex);
        m_tex = nullptr;
    }
}

namespace {

struct Geo {
    float sx, sy, top;
    float X(float e) const { return e * sx; }
    float Y(float e) const { return e * sy - top; }
    float W(float e) const { return e * sx; }
    float H(float e) const { return e * sy; }
};

struct Dial {
    float cx, cy, R;   // R: the face; the bezel is outside it
};

Dial dialOf(const Geo& g, const Box& b) {
    const float w = g.W(b.x1 - b.x0), h = g.H(b.y1 - b.y0);
    return {g.X((b.x0 + b.x1) * 0.5f), g.Y((b.y0 + b.y1) * 0.5f), std::min(w, h) * 0.5f / 1.13f - 1.f};
}

void dialFace(Cv& cv, const Dial& D) {
    const float R = D.R;
    cv.disc(D.cx + R * 0.04f, D.cy + R * 0.07f, R * 1.13f, Col{0, 0, 0}, 0.4f);   // a soft shadow
    cv.disc(D.cx, D.cy, R * 1.12f, [&](float x, float y) {
        const float t = sat(((x - D.cx) + (y - D.cy)) / (R * 2.2f) * 0.5f + 0.5f);
        return mix(C_BEZEL_LT, C_BEZEL_DK, t);
    });
    cv.ring(D.cx, D.cy, R * 1.03f, R * 0.05f, rgb(0x08090b));
    cv.disc(D.cx, D.cy, R, [&](float x, float y) {
        const float t = sat(std::hypot(x - D.cx, y - D.cy) / R);
        return mix(C_FACE_IN, C_FACE_OUT, t * t);
    });
}

// the glass: a faint highlight on the upper left
void dialGlass(Cv& cv, const Dial& D) {
    cv.arc(D.cx, D.cy, D.R * 0.86f, -2.6f, -1.0f, D.R * 0.05f, Col{1, 1, 1}, 0.10f);
}

void scaleTicks(Cv& cv, const Dial& D, float a0, float a1, int n, int majorEvery, float rOut, float majLen, float minLen, float wMaj, float wMin,
                Col c) {
    for (int i = 0; i <= n; i++) {
        const float a = a0 + (a1 - a0) * i / n;
        const bool major = (i % majorEvery) == 0;
        const float len = major ? majLen : minLen;
        float x0, y0, x1, y1;
        polar(D.cx, D.cy, rOut, a, x0, y0);
        polar(D.cx, D.cy, rOut - len, a, x1, y1);
        cv.line(x0, y0, x1, y1, major ? wMaj : wMin, c);
    }
}

void lampOff(Cv& cv, float x0, float y0, float x1, float y1, const char* label, float th) {
    cv.rrect(x0, y0, x1, y1, (y1 - y0) * 0.22f, [&](float, float y) { return mix(rgb(0x2a2d34), rgb(0x1a1c21), (y - y0) / (y1 - y0)); });
    cv.rrectLine(x0, y0, x1, y1, (y1 - y0) * 0.22f, 1.2f, C_LAMP_EDGE);
    text(cv, label, (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, th, C_LAMP_TXT);
}

void lampOn(Cv& cv, float x0, float y0, float x1, float y1, const char* label, float th, Col c, Col t = rgb(0xffffff)) {
    cv.rrect(x0 - 2, y0 - 2, x1 + 2, y1 + 2, (y1 - y0) * 0.3f, c, 0.22f);   // the glow
    cv.rrect(x0, y0, x1, y1, (y1 - y0) * 0.22f, [&](float, float y) { return mix(scaled(c, 1.15f), scaled(c, 0.72f), (y - y0) / (y1 - y0)); });
    cv.rrectLine(x0, y0, x1, y1, (y1 - y0) * 0.22f, 1.2f, scaled(c, 1.3f), 0.8f);
    text(cv, label, (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, th, t);
}

} // namespace

void ModernPanel::buildStatic() {
    Cv cv{m_static.px.data(), m_static.w, m_static.h};
    const Geo g{m_sx, m_sy, static_cast<float>(m_top)};
    cv.fill(C_PANEL_TOP, C_PANEL_BOT);
    cv.line(0, 1.f, static_cast<float>(cv.w), 1.f, 2.f, rgb(0x5a606b));   // the top edge catches the light
    cv.line(0, 3.f, static_cast<float>(cv.w), 3.f, 1.f, rgb(0x0a0b0d));

    const float lh = g.H(5.5f);   // text height of the labels

    // ---- the big dials ----
    {   // altimeter: 100 feet a turn, the second hand 1000, the third 10000
        const Dial D = dialOf(g, BOX_ALT);
        dialFace(cv, D);
        scaleTicks(cv, D, 0, TAU, 100, 10, D.R * 0.95f, D.R * 0.16f, D.R * 0.07f, std::max(1.6f, D.R * 0.035f), 1.f, C_TICK);
        for (int k = 0; k < 10; k++) {
            float x, y;
            char s[2] = {static_cast<char>('0' + k), 0};
            polar(D.cx, D.cy, D.R * 0.6f, k * TAU / 10, x, y);
            text(cv, s, x, y, D.R * 0.24f, C_TEXT);
        }
        text(cv, "ALT", D.cx, D.cy - D.R * 0.34f, D.R * 0.15f, C_DIM);
        text(cv, "x100 FT", D.cx, D.cy + D.R * 0.36f, D.R * 0.13f, C_DIM);
        dialGlass(cv, D);
    }
    {   // airspeed: a turn is 500 mph, numerals are hundreds
        const Dial D = dialOf(g, BOX_SPD);
        dialFace(cv, D);
        scaleTicks(cv, D, 0, TAU, 50, 10, D.R * 0.95f, D.R * 0.16f, D.R * 0.07f, std::max(1.6f, D.R * 0.035f), 1.f, C_TICK);
        for (int k = 0; k < 5; k++) {
            float x, y;
            char s[2] = {static_cast<char>('0' + k), 0};
            polar(D.cx, D.cy, D.R * 0.6f, k * TAU / 5, x, y);
            text(cv, s, x, y, D.R * 0.26f, C_TEXT);
        }
        // stall speed (100 mph) marker
        cv.arc(D.cx, D.cy, D.R * 0.97f, 0.f, 0.2f * TAU * 0.0f + 0.001f, 1.f, C_RED);
        text(cv, "MPH", D.cx, D.cy - D.R * 0.34f, D.R * 0.15f, C_DIM);
        text(cv, "x100", D.cx, D.cy + D.R * 0.36f, D.R * 0.13f, C_DIM);
        dialGlass(cv, D);
    }
    {   // rate of climb: the angles of INST.ASM: 150 degrees (full dive) to 390 degrees (full climb), level at 270
        const Dial D = dialOf(g, BOX_CLM);
        dialFace(cv, D);
        const float a0 = 150.f / 360.f * TAU, a1 = 390.f / 360.f * TAU, aMid = 270.f / 360.f * TAU;
        cv.arc(D.cx, D.cy, D.R * 0.9f, a0, aMid - 0.02f, D.R * 0.08f, C_RED, 0.75f);
        cv.arc(D.cx, D.cy, D.R * 0.9f, aMid + 0.02f, a1, D.R * 0.08f, C_GREEN, 0.75f);
        scaleTicks(cv, D, a0, a1, 16, 4, D.R * 0.82f, D.R * 0.16f, D.R * 0.08f, std::max(1.6f, D.R * 0.035f), 1.f, C_TICK);
        float x, y;
        polar(D.cx, D.cy, D.R * 0.44f, aMid, x, y);
        text(cv, "0", x, y, D.R * 0.22f, C_TEXT);
        polar(D.cx, D.cy, D.R * 0.55f, 0.f, x, y);
        text(cv, "UP", x, y, D.R * 0.2f, C_TEXT);
        polar(D.cx, D.cy, D.R * 0.55f, PI, x, y);
        text(cv, "DN", x, y, D.R * 0.2f, C_TEXT);
        dialGlass(cv, D);
    }
    {   // attitude: the face is painted live, the ring and the bank scale are fixed
        const Dial D = dialOf(g, BOX_ATT);
        dialFace(cv, D);
        const float ro = D.R * 1.0f;
        const float marks[] = {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60};
        for (float m : marks) {
            const float a = m * PI / 180.f;
            const bool big = std::fabs(m) == 60 || std::fabs(m) == 30 || m == 0;
            float x0, y0, x1, y1;
            polar(D.cx, D.cy, ro * 0.99f, a, x0, y0);
            polar(D.cx, D.cy, ro * (big ? 0.86f : 0.92f), a, x1, y1);
            cv.line(x0, y0, x1, y1, big ? std::max(1.8f, D.R * 0.04f) : 1.2f, C_TICK);
        }
    }

    // ---- the small dials of the top row ----
    {   // engine temperature: a sector from cold to hot
        const Dial D = dialOf(g, BOX_TEMP);
        const float cx = g.X(32), cy = g.Y(234), R = g.H(25.f);
        cv.rrect(g.X(BOX_TEMP.x0), g.Y(BOX_TEMP.y0), g.X(BOX_TEMP.x1), g.Y(BOX_TEMP.y1), 5.f, rgb(0x14171c));
        cv.rrectLine(g.X(BOX_TEMP.x0), g.Y(BOX_TEMP.y0), g.X(BOX_TEMP.x1), g.Y(BOX_TEMP.y1), 5.f, 1.4f, C_BEZEL_DK);
        const float a0 = -33.f * PI / 180.f, a1 = 33.f * PI / 180.f;
        cv.arc(cx, cy, R * 0.9f, a0, a1 * 0.2f, R * 0.09f, C_BLUE, 0.9f);
        cv.arc(cx, cy, R * 0.9f, a1 * 0.2f, a1 * 0.62f, R * 0.09f, C_GREEN, 0.9f);
        cv.arc(cx, cy, R * 0.9f, a1 * 0.62f, a1, R * 0.09f, C_RED, 0.9f);
        scaleTicks(cv, Dial{cx, cy, R}, a0, a1, 8, 4, R * 0.78f, R * 0.14f, R * 0.07f, 1.8f, 1.f, C_TICK);
        float x, y;
        polar(cx, cy, R * 0.52f, a0 - 0.12f, x, y);
        text(cv, "C", x, y, lh * 1.05f, C_TEXT);
        polar(cx, cy, R * 0.52f, a1 + 0.12f, x, y);
        text(cv, "H", x, y, lh * 1.05f, C_TEXT);
        (void)D;
    }
    {   // oil pressure: 240 degrees, from -150 (0) to +90 degrees
        const Dial D = dialOf(g, BOX_OIL);
        dialFace(cv, D);
        const float a0 = -150.f * PI / 180.f, a1 = 90.f * PI / 180.f;
        cv.arc(D.cx, D.cy, D.R * 0.88f, a0, a0 + (a1 - a0) * 0.22f, D.R * 0.09f, C_RED, 0.85f);
        cv.arc(D.cx, D.cy, D.R * 0.88f, a0 + (a1 - a0) * 0.22f, a1, D.R * 0.09f, C_GREEN, 0.85f);
        scaleTicks(cv, D, a0, a1, 8, 4, D.R * 0.78f, D.R * 0.15f, D.R * 0.08f, 1.8f, 1.f, C_TICK);
        text(cv, "OIL", D.cx, D.cy + D.R * 0.42f, D.R * 0.24f, C_DIM);
        dialGlass(cv, D);
    }
    {   // compass: the card turns; the lubber line is fixed
        const Dial D = dialOf(g, BOX_CMP);
        dialFace(cv, D);
        float x, y;
        polar(D.cx, D.cy, D.R * 1.0f, 0.f, x, y);
        const float tri[] = {x, y + D.R * 0.02f, x - D.R * 0.1f, y - D.R * 0.17f, x + D.R * 0.1f, y - D.R * 0.17f};
        cv.poly(tri, 3, C_ORANGE);
    }
    {   // the readout box
        const Box& b = BOX_PIT;
        cv.rrect(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 5.f, rgb(0x0d0f12));
        cv.rrectLine(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 5.f, 1.4f, C_BEZEL_DK);
        text(cv, "HDG", g.X(b.x0 + 3), g.Y(b.y0 + 6.5f), lh * 0.72f, C_DIM, -1);
        text(cv, "PIT", g.X(b.x0 + 3), g.Y(b.y0 + 16.5f), lh * 0.72f, C_DIM, -1);
        text(cv, "MPH", g.X(b.x0 + 3), g.Y(b.y0 + 26.5f), lh * 0.72f, C_DIM, -1);
    }

    // ---- lamps: off state ----
    {
        const Box& b = BOX_L1;
        cv.rrect(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 5.f, rgb(0x14171c));
        cv.rrectLine(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 5.f, 1.4f, C_BEZEL_DK);
        const char* names[3] = {"AUTO", "HOLD", "FLAPS"};
        for (int i = 0; i < 3; i++) {
            const float y0 = b.y0 + 2.5f + i * 10.6f;
            lampOff(cv, g.X(b.x0 + 3), g.Y(y0), g.X(b.x1 - 3), g.Y(y0 + 9.4f), names[i], g.H(5.6f));
        }
    }
    {
        const Box& b = BOX_L2;
        cv.rrect(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 5.f, rgb(0x14171c));
        cv.rrectLine(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 5.f, 1.4f, C_BEZEL_DK);
        const char* names[3] = {"STALL", "EJECT", "CMPLT"};
        for (int i = 0; i < 3; i++) {
            const float y0 = b.y0 + 2.5f + i * 10.6f;
            lampOff(cv, g.X(b.x0 + 3), g.Y(y0), g.X(b.x1 - 3), g.Y(y0 + 9.4f), names[i], g.H(5.6f));
        }
    }
    {   // the middle box: altitude warning and the altitude readout
        const Box& b = BOX_ALW;
        cv.rrect(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 5.f, rgb(0x0d0f12));
        cv.rrectLine(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 5.f, 1.4f, C_BEZEL_DK);
        lampOff(cv, g.X(b.x0 + 5), g.Y(b.y0 + 3), g.X(b.x0 + 55), g.Y(b.y0 + 15), "LOW ALT", g.H(4.6f));
        text(cv, "FEET", g.X(b.x1 - 5), g.Y(b.y0 + 8.f), g.H(3.6f), C_DIM, 1);
    }
    {   // control positions
        const Box& b = BOX_CTL;
        cv.rrect(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 5.f, rgb(0x0d0f12));
        cv.rrectLine(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 5.f, 1.4f, C_BEZEL_DK);
        const float ex = g.X(368);
        cv.line(ex, g.Y(212), ex, g.Y(237), 2.f, rgb(0x39404a));
        for (int i = 0; i <= 4; i++) cv.line(ex - g.W(3), g.Y(212 + i * 6.25f), ex + g.W(3), g.Y(212 + i * 6.25f), 1.f, rgb(0x59616d));
        for (int row = 0; row < 2; row++) {
            const float yy = g.Y(row == 0 ? 216.f : 232.f);
            cv.line(g.X(385), yy, g.X(429), yy, 2.f, rgb(0x39404a));
            for (int i = 0; i <= 4; i++) cv.line(g.X(385 + i * 11.f), yy - g.H(2.2f), g.X(385 + i * 11.f), yy + g.H(2.2f), 1.f, rgb(0x59616d));
        }
        text(cv, "ELEV", g.X(368), g.Y(240.f), g.H(3.8f), C_DIM);
        text(cv, "RUDDER", g.X(407), g.Y(221.5f), g.H(3.8f), C_DIM);
        text(cv, "AILERON", g.X(407), g.Y(237.5f), g.H(3.8f), C_DIM);
    }
    {   // the bombs
        const Box& b = BOX_BMB;
        cv.rrect(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 5.f, rgb(0x14171c));
        cv.rrectLine(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 5.f, 1.4f, C_BEZEL_DK);
    }
    {   // damage bars
        const Box& b = BOX_DMG;
        cv.rrect(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 5.f, rgb(0x0d0f12));
        cv.rrectLine(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 5.f, 1.4f, C_BEZEL_DK);
        const char* names[4] = {"ENG", "WING", "STAB", "RUD"};
        for (int i = 0; i < 4; i++) {
            const float yy = g.Y(DMG_Y[i] + 0.5f);
            text(cv, names[i], g.X(b.x0 + 4), yy, g.H(4.4f), C_DIM, -1);
            for (int k = 0; k < 13; k++) {
                const float x0 = g.X(DMG_X + 19 + k * 5.0f), x1 = g.X(DMG_X + 19 + k * 5.0f + 3.6f);
                cv.rrect(x0, yy - g.H(2.0f), x1, yy + g.H(2.0f), 1.6f, rgb(0x1f2329));
            }
        }
    }
    {   // throttle
        const Box& b = BOX_THR;
        cv.rrect(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 4.f, rgb(0x0d0f12));
        cv.rrectLine(g.X(b.x0), g.Y(b.y0), g.X(b.x1), g.Y(b.y1), 4.f, 1.4f, C_BEZEL_DK);
        text(cv, "THR", g.X(b.x0 + 3), g.Y((b.y0 + b.y1) * 0.5f), g.H(3.8f), C_DIM, -1);
        cv.rrect(g.X(b.x0 + 44), g.Y(b.y0 + 2.5f), g.X(b.x1 - 5), g.Y(b.y1 - 2.5f), 3.f, rgb(0x1f2329));
    }

    // ---- the two windows of the EGA picture ----
    for (const Box* b : {&REAR_FRAME, &RAD_FRAME}) {
        const bool rear = b == &REAR_FRAME;
        const Box& hole = rear ? REAR_HOLE : RAD_HOLE;
        cv.rrect(g.X(b->x0), g.Y(b->y0), g.X(b->x1), g.Y(b->y1), 7.f, [&](float x, float y) {
            const float t = sat(((x - g.X(b->x0)) + (y - g.Y(b->y0))) / (g.X(b->x1 - b->x0) + g.H(b->y1 - b->y0)));
            return mix(rgb(0x59606b), rgb(0x20232a), t);
        });
        cv.rrectLine(g.X(b->x0), g.Y(b->y0), g.X(b->x1), g.Y(b->y1), 7.f, 1.4f, rgb(0x0a0b0d));
        if (!rear) {   // the radar's own black glass
            cv.rrect(g.X(hole.x0 - 4), g.Y(hole.y0 - 4), g.X(hole.x1 + 4), g.Y(hole.y1 + 4), 6.f, rgb(0x050607));
            text(cv, "RADAR", g.X((b->x0 + b->x1) * 0.5f), g.Y(b->y1 - 5.f), g.H(4.2f), C_DIM);
        }
        SDL_Rect r;
        r.x = static_cast<int>(std::ceil(g.X(hole.x0)));
        r.y = static_cast<int>(std::ceil(g.Y(hole.y0)));
        r.w = static_cast<int>(std::floor(g.X(hole.x1))) - r.x;
        r.h = static_cast<int>(std::floor(g.Y(hole.y1))) - r.y;
        m_holes.push_back(r);
    }
    for (const SDL_Rect& r : m_holes)
        for (int y = std::max(0, r.y); y < std::min(cv.h, r.y + r.h); y++)
            for (int x = std::max(0, r.x); x < std::min(cv.w, r.x + r.w); x++) cv.p[static_cast<size_t>(y) * cv.w + x] = 0;
}

void ModernPanel::paint(const EgaScreen& scr, const EgaPalette& pal) {
    m_frame.px = m_static.px;
    Cv cv{m_frame.px.data(), m_frame.w, m_frame.h};
    const Geo g{m_sx, m_sy, static_cast<float>(m_top)};
    auto W = [&](uint32_t o) { return static_cast<int>(m_g.w(o)); };
    auto UW = [&](uint32_t o) { return static_cast<uint32_t>(m_g.uw(o)); };
    const float lh = g.H(5.5f);

    // ---- reading the original panel back: the lit lamps, the bombs, the damage bars ----
    const int s = scr.scale(), sw = scr.width();
    const uint8_t* ega = scr.front();
    auto count = [&](const Box& b, int color) {
        int n = 0;
        for (int y = static_cast<int>(b.y0); y <= static_cast<int>(b.y1); y++)
            for (int x = static_cast<int>(b.x0); x <= static_cast<int>(b.x1); x++)
                if ((ega[static_cast<size_t>(y * s + s / 2) * sw + (x * s + s / 2)] & 15) == color) n++;
        return n;
    };
    auto sample = [&](int x, int y) { return ega[static_cast<size_t>(y * s + s / 2) * sw + (x * s + s / 2)] & 15; };

    // ---- the values ----
    const double yaw = static_cast<uint16_t>(W(lbl::orvec)) / 65536.0;
    const float pitchUp = -static_cast<int16_t>(W(lbl::orvec + 2)) * 360.f / 65536.f;   // degrees, nose up positive
    const float roll = static_cast<int16_t>(W(lbl::orvec + 4)) * TAU / 65536.f;
    uint32_t speed = static_cast<uint16_t>(W(lbl::topv));
    if (UW(lbl::obj5dbits) & 0x2000) speed <<= 2;
    int32_t altUnits = m_g.dw(lbl::oz) - static_cast<int32_t>(static_cast<uint16_t>(W(lbl::hmin)));
    if (altUnits < 0) altUnits = 0;
    const double altFt = altUnits / 30.5;   // a turn of the fast hand is 3050 units = 100 feet

    // ---- altimeter ----
    {
        const Dial D = dialOf(g, BOX_ALT);
        const double turns = altUnits / 3050.0;
        hand(cv, D.cx, D.cy, turn16(turns / 100.0), D.R * 0.42f, D.R * 0.08f, std::max(3.f, D.R * 0.11f), C_CYAN);
        hand(cv, D.cx, D.cy, turn16(turns / 10.0), D.R * 0.58f, D.R * 0.1f, std::max(3.f, D.R * 0.1f), C_ORANGE);
        hand(cv, D.cx, D.cy, turn16(turns), D.R * 0.9f, D.R * 0.16f, std::max(2.5f, D.R * 0.07f), C_HAND);
        cv.disc(D.cx, D.cy, D.R * 0.09f, [&](float x, float y) { return mix(rgb(0xd9dde3), rgb(0x59606b), sat(((x - D.cx) + (y - D.cy)) / (D.R * 0.2f) * 0.5f + 0.5f)); });
        // the lights for 10000 and 20000 feet
        const Col on = rgb(0xff4fd8);
        for (int i = 0; i < 2; i++) {
            const bool lit = count(BTN[10 + i], 5) > 4;
            const float px = g.X(BOX_ALT.x1 - 3.5f), py = g.Y(BOX_ALT.y0 + 6.f + i * 6.5f);
            cv.disc(px, py, g.H(2.0f), lit ? on : rgb(0x2a2d34));
            if (lit) cv.disc(px, py, g.H(3.4f), on, 0.25f);
        }
    }    // ---- airspeed ----
    {
        const Dial D = dialOf(g, BOX_SPD);
        const uint32_t rem = speed % 19648;
        hand(cv, D.cx, D.cy, turn16(rem / 19648.0), D.R * 0.9f, D.R * 0.16f, std::max(3.f, D.R * 0.09f), C_HAND);
        cv.disc(D.cx, D.cy, D.R * 0.09f, [&](float x, float y) { return mix(rgb(0xd9dde3), rgb(0x59606b), sat(((x - D.cx) + (y - D.cy)) / (D.R * 0.2f) * 0.5f + 0.5f)); });
    }
    // ---- climb ----
    {
        const Dial D = dialOf(g, BOX_CLM);
        int dx = W(lbl::rtopv + 4) + 804;
        dx = std::clamp(dx, 0, 1607);
        const float ang = (150.f + dx / 2414.f * 360.f) * PI / 180.f;
        hand(cv, D.cx, D.cy, ang, D.R * 0.86f, D.R * 0.16f, std::max(3.f, D.R * 0.09f), C_HAND);
        cv.disc(D.cx, D.cy, D.R * 0.09f, [&](float x, float y) { return mix(rgb(0xd9dde3), rgb(0x59606b), sat(((x - D.cx) + (y - D.cy)) / (D.R * 0.2f) * 0.5f + 0.5f)); });
    }
    // ---- attitude ----
    {
        const Dial D = dialOf(g, BOX_ATT);
        const float R = D.R * 0.86f;
        const float ppd = R * 0.024f;                       // pixels per degree of pitch
        const float nx = std::sin(roll), ny = std::cos(roll);   // towards the ground, turned with the roll
        const float tx = ny, ty = -nx;                          // along the horizon
        const float off = pitchUp * ppd;                        // nose up: the horizon goes down
        auto sd = [&](float x, float y) { return std::hypot(x - D.cx, y - D.cy) - R; };
        cv.shape(D.cx - R - 1, D.cy - R - 1, D.cx + R + 1, D.cy + R + 1, sd, [&](float x, float y) {
            const float v = (x - D.cx) * nx + (y - D.cy) * ny - off;   // > 0: ground
            const float u = (x - D.cx) * tx + (y - D.cy) * ty;
            const float a = sat(0.5f + v);                              // the horizon edge, anti-aliased
            const float dist = std::fabs(v);
            Col sky = mix(C_SKY_LO, C_SKY_HI, sat(-v / (R * 1.1f)));
            Col gnd = mix(C_GND_HI, C_GND_LO, sat(v / (R * 1.1f)));
            Col c = mix(sky, gnd, a);
            if (dist < 1.1f) c = mix(c, Col{1, 1, 1}, 0.9f * (1.f - dist / 1.1f));   // the horizon line
            // pitch ladder
            for (int k : {-30, -20, -10, 10, 20, 30}) {
                const float lv = v + k * ppd;   // v of the rung = k * ppd (nose up shows the rungs below the horizon)
                const float half = (std::abs(k) % 20 == 0 ? 0.32f : 0.22f) * R;
                if (std::fabs(lv) < 0.9f && std::fabs(u) < half && std::fabs(u) > R * 0.07f) c = mix(c, Col{1, 1, 1}, 0.85f);
            }
            return c;
        });
        // the shading of the glass
        cv.ring(D.cx, D.cy, R, R * 0.07f, Col{0, 0, 0}, 0.45f);
        // the bank pointer rides the sky
        {
            float x, y;
            polar(D.cx, D.cy, D.R * 0.99f, -roll, x, y);
            const float a = -roll;
            const float ex = std::cos(a), ey = std::sin(a);   // across the pointer
            const float sx = std::sin(a), sy = -std::cos(a);  // outwards
            const float h = D.R * 0.13f;
            const float p0x = x, p0y = y;
            const float p1x = x - sx * h + ex * h * 0.55f, p1y = y - sy * h + ey * h * 0.55f;
            const float p2x = x - sx * h - ex * h * 0.55f, p2y = y - sy * h - ey * h * 0.55f;
            const float t3[] = {p0x, p0y, p1x, p1y, p2x, p2y};
            cv.poly(t3, 3, C_ORANGE);
        }
        // the aircraft symbol
        const float w = std::max(2.4f, D.R * 0.06f);
        cv.line(D.cx - R * 0.62f, D.cy, D.cx - R * 0.22f, D.cy, w, C_ORANGE);
        cv.line(D.cx - R * 0.22f, D.cy, D.cx - R * 0.22f, D.cy + R * 0.12f, w, C_ORANGE);
        cv.line(D.cx + R * 0.62f, D.cy, D.cx + R * 0.22f, D.cy, w, C_ORANGE);
        cv.line(D.cx + R * 0.22f, D.cy, D.cx + R * 0.22f, D.cy + R * 0.12f, w, C_ORANGE);
        cv.disc(D.cx, D.cy, w * 0.85f, C_ORANGE);
    }
    // ---- engine temperature ----
    {
        const float cx = g.X(32), cy = g.Y(234), R = g.H(25.f);
        const float t = std::clamp(static_cast<float>(UW(lbl::etemp)) / 32767.f, 0.f, 1.f);
        const float a = (-33.f + 66.f * t) * PI / 180.f;
        hand(cv, cx, cy, a, R * 0.88f, R * 0.12f, std::max(2.6f, R * 0.1f), C_HAND);
        cv.disc(cx, cy, R * 0.1f, rgb(0xb7bec8));
    }
    // ---- oil pressure ----
    {
        const Dial D = dialOf(g, BOX_OIL);
        const float t = std::clamp(static_cast<float>(UW(lbl::opres)) / 32767.f, 0.f, 1.f);
        const float a = (-150.f + 240.f * t) * PI / 180.f;
        hand(cv, D.cx, D.cy, a, D.R * 0.8f, D.R * 0.14f, std::max(2.6f, D.R * 0.12f), C_HAND);
        cv.disc(D.cx, D.cy, D.R * 0.1f, rgb(0xb7bec8));
    }
    // ---- compass ----
    {
        const Dial D = dialOf(g, BOX_CMP);
        const float rot = -static_cast<float>(yaw) * TAU;   // where north is
        const float r1 = D.R * 0.96f;
        for (int i = 0; i < 72; i++) {
            const float a = rot + i * TAU / 72.f;
            const bool major = (i % 6) == 0;
            float x0, y0, x1, y1;
            polar(D.cx, D.cy, r1, a, x0, y0);
            polar(D.cx, D.cy, r1 - D.R * (major ? 0.16f : 0.08f), a, x1, y1);
            cv.line(x0, y0, x1, y1, major ? std::max(1.6f, D.R * 0.045f) : 1.f, C_TICK);
        }
        const char* card[4] = {"N", "E", "S", "W"};
        for (int k = 0; k < 4; k++) {
            float x, y;
            polar(D.cx, D.cy, D.R * 0.58f, rot + k * TAU / 4.f, x, y);
            text(cv, card[k], x, y, D.R * 0.27f, k == 0 ? C_ORANGE : C_TEXT);
        }
        // the plane in the middle
        cv.line(D.cx, D.cy - D.R * 0.2f, D.cx, D.cy + D.R * 0.2f, std::max(1.8f, D.R * 0.06f), C_HAND);
        cv.line(D.cx - D.R * 0.16f, D.cy + D.R * 0.02f, D.cx + D.R * 0.16f, D.cy + D.R * 0.02f, std::max(1.8f, D.R * 0.06f), C_HAND);
        dialGlass(cv, D);
    }
    // ---- the readouts ----
    {
        const Box& b = BOX_PIT;
        char buf[24];
        int hdg = static_cast<int>(std::lround(yaw * 360.0)) % 360;
        if (hdg < 0) hdg += 360;
        std::snprintf(buf, sizeof buf, "%03d^", hdg);
        text(cv, buf, g.X(b.x1 - 3), g.Y(b.y0 + 6.5f), lh * 0.9f, C_TEXT, 1);
        std::snprintf(buf, sizeof buf, "%+d^", static_cast<int>(std::lround(pitchUp)));
        text(cv, buf, g.X(b.x1 - 3), g.Y(b.y0 + 16.5f), lh * 0.9f, C_TEXT, 1);
        std::snprintf(buf, sizeof buf, "%d", static_cast<int>(std::lround(speed * 500.0 / 19648.0)));
        text(cv, buf, g.X(b.x1 - 3), g.Y(b.y0 + 26.5f), lh * 0.9f, C_TEXT, 1);
        const Box& a = BOX_ALW;
        std::snprintf(buf, sizeof buf, "%d", static_cast<int>(std::lround(altFt)));
        text(cv, buf, g.X(a.x1 - 5), g.Y(a.y0 + 19.f), g.H(8.5f), C_TEXT, 1);
    }

    // ---- lamps ----
    {
        struct L { Box where; int btn; const char* label; Col c; int litColor; int inBox; int idx; };
        const Box b1 = BOX_L1, b2 = BOX_L2;
        auto row = [&](const Box& b, int i) { const float y0 = b.y0 + 2.5f + i * 10.6f; return Box{b.x0 + 3, y0, b.x1 - 3, y0 + 9.4f}; };
        auto lit = [&](const Box& r, const char* label, Col c, Col t = rgb(0xffffff)) {
            lampOn(cv, g.X(r.x0), g.Y(r.y0), g.X(r.x1), g.Y(r.y1), label, g.H(5.6f), c, t);
        };
        if (count(BTN[0], 1) > 20) lit(row(b1, 0), "AUTO", C_BLUE);
        if (count(BTN[1], 5) > 20 || count(BTN[1], 15) > 40) lit(row(b1, 1), "HOLD", rgb(0xc040d0));
        if (count(BTN[2], 6) > 20) lit(row(b1, 2), "FLAPS", rgb(0xb0702a));
        if (count(BTN[3], 12) > 20) lit(row(b2, 0), "STALL", C_RED, rgb(0xffe27a));
        if (count(BTN[4], 4) > 20) lit(row(b2, 1), "EJECT", C_RED);
        if (count(BTN[5], 9) > 20) lit(row(b2, 2), "CMPLT", rgb(0x2f9bff));
        // low altitude: flashes yellow
        if (count(BTN[9], 14) > 20 || count(BTN[9], 12) > 20) {
            const Box& a = BOX_ALW;
            lampOn(cv, g.X(a.x0 + 5), g.Y(a.y0 + 3), g.X(a.x0 + 55), g.Y(a.y0 + 15), "LOW ALT", g.H(4.6f), C_AMBER, rgb(0x201000));
        }
        // the help prompt flashes until the throttle is touched
        if (m_g.b(d::m3::freezeflag) != 0 && m_g.b(d::m3::helpflashflag) != 0 && count(BTN[12], 7) > 6) {
            const Box& a = BOX_ALW;
            text(cv, "PRESS + FOR POWER", g.X((a.x0 + a.x1) * 0.5f), g.Y(a.y1 - 4.f), g.H(3.6f), C_BLUE);
        } else {
            const Box& a = BOX_ALW;
            text(cv, "F4 FOR HELP", g.X((a.x0 + a.x1) * 0.5f), g.Y(a.y1 - 4.f), g.H(3.6f), C_DIM);
        }
        // the bombs
        for (int i = 0; i < 3; i++) {
            const bool have = count(BTN[6 + i], 8) > 3;
            const float cx = g.X((BOX_BMB.x0 + BOX_BMB.x1) * 0.5f), cy = g.Y(BOX_BMB.y0 + 6.f + i * 10.2f);
            const float bl = g.W(10.5f), bh = g.H(3.3f);
            const Col body = have ? rgb(0xb7bfca) : rgb(0x262a31), fin = have ? rgb(0x8d95a1) : rgb(0x20232a);
            cv.rrect(cx - bl * 0.5f, cy - bh * 0.5f, cx + bl * 0.5f, cy + bh * 0.5f, bh * 0.5f, body);
            const float f[] = {cx + bl * 0.5f, cy, cx + bl * 0.5f - g.W(3.4f), cy - bh * 0.95f, cx + bl * 0.5f - g.W(3.4f), cy + bh * 0.95f};
            cv.poly(f, 3, fin);
            if (have) cv.line(cx - bl * 0.18f, cy - bh * 0.3f, cx + bl * 0.1f, cy - bh * 0.3f, 1.2f, Col{1, 1, 1}, 0.45f);
        }
    }
    // ---- control positions ----
    {
        auto frac = [&](int v, bool neg) { float t = ((neg ? -v : v) + 512) / 1024.f; return std::clamp(t, 0.f, 1.f); };
        const float ey = g.Y(212.f + frac(W(lbl::yang), true) * 25.f);
        cv.rrect(g.X(368) - g.W(3.6f), ey - g.H(1.2f), g.X(368) + g.W(3.6f), ey + g.H(1.2f), 2.f, C_AMBER);
        const float rx = g.X(385.f + frac(W(lbl::rdrang), true) * 44.f), ax = g.X(385.f + frac(W(lbl::xang), false) * 44.f);
        cv.rrect(rx - g.W(1.0f), g.Y(216) - g.H(2.8f), rx + g.W(1.0f), g.Y(216) + g.H(2.8f), 2.f, C_AMBER);
        cv.rrect(ax - g.W(1.0f), g.Y(232) - g.H(2.8f), ax + g.W(1.0f), g.Y(232) + g.H(2.8f), 2.f, C_AMBER);
    }
    // ---- damage bars: the original segment colors ----
    for (int i = 0; i < 4; i++) {
        const float yy = g.Y(DMG_Y[i] + 0.5f);
        for (int k = 0; k < 13; k++) {
            const int c = sample(DMG_X + 6 * k + 2, DMG_Y[i]);
            if (c == 0 || c == 7 || c == 8 || c == 15) continue;
            const Rgb rgbv = pal.rgb(static_cast<uint8_t>(c));
            const Col col{rgbv.r / 255.f, rgbv.g / 255.f, rgbv.b / 255.f};
            const float x0 = g.X(DMG_X + 19 + k * 5.0f), x1 = g.X(DMG_X + 19 + k * 5.0f + 3.6f);
            cv.rrect(x0, yy - g.H(2.0f), x1, yy + g.H(2.0f), 1.6f, col);
        }
    }
    // ---- throttle ----
    {
        const Box& b = BOX_THR;
        const float rpm = static_cast<float>(W(lbl::rpm)), rpmx = static_cast<float>(std::max(1, W(lbl::rpmx)));
        const float t = std::clamp(rpm / rpmx, 0.f, 1.f);
        const float x0 = g.X(b.x0 + 44), x1 = g.X(b.x1 - 5), y0 = g.Y(b.y0 + 2.5f), y1 = g.Y(b.y1 - 2.5f);
        if (t > 0.01f)
            cv.rrect(x0, y0, x0 + (x1 - x0) * t, y1, 3.f, [&](float x, float) { return mix(C_GREEN, t > 0.9f ? C_AMBER : C_GREEN, sat((x - x0) / (x1 - x0))); });
        char buf[8];
        std::snprintf(buf, sizeof buf, "%d%%", static_cast<int>(std::lround(t * 100)));
        text(cv, buf, g.X(b.x0 + 41), g.Y((b.y0 + b.y1) * 0.5f), g.H(3.8f), C_TEXT, 1);
    }
}

void ModernPanel::draw(SDL_Renderer* sdl, const SDL_Rect& dst, const EgaScreen& scr, const EgaPalette& pal) {
    if (!active()) return;
    if (sdl != m_sdl) {
        if (m_tex) SDL_DestroyTexture(m_tex);
        m_tex = nullptr;
        m_sdl = sdl;
        m_w = 0;
    }
    ensure(dst.w, dst.h);
    paint(scr, pal);
    if (!m_tex) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        m_tex = SDL_CreateTexture(sdl, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, m_frame.w, m_frame.h);
        if (m_tex) SDL_SetTextureBlendMode(m_tex, SDL_BLENDMODE_BLEND);
    }
    if (!m_tex) return;
    SDL_UpdateTexture(m_tex, nullptr, m_frame.px.data(), m_frame.w * 4);
    const SDL_Rect to{dst.x, dst.y + m_top, m_frame.w, m_frame.h};
    SDL_RenderCopy(sdl, m_tex, nullptr, &to);
}

bool ModernPanel::saveComposite(const EgaScreen& scr, const EgaPalette& pal, const char* path, int w, int h) {
    ensure(w, h);
    paint(scr, pal);
    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!sf) return false;
    const uint8_t* src = scr.front();
    for (int y = 0; y < h; y++) {
        uint32_t* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(sf->pixels) + y * sf->pitch);
        const int sy = std::min(scr.height() - 1, y * scr.height() / h);
        for (int x = 0; x < w; x++) {
            const int sx = std::min(scr.width() - 1, x * scr.width() / w);
            const Rgb c = pal.rgb(src[static_cast<size_t>(sy) * scr.width() + sx]);
            row[x] = 0xFF000000u | (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b;
        }
    }
    if (active())
        for (int y = 0; y < m_frame.h; y++) {
            uint32_t* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(sf->pixels) + (y + m_top) * sf->pitch);
            for (int x = 0; x < m_frame.w; x++) {
                const uint32_t v = m_frame.px[static_cast<size_t>(y) * m_frame.w + x];
                if (v >> 24) row[x] = v;
            }
        }
    const bool ok = SDL_SaveBMP(sf, path) == 0;
    SDL_FreeSurface(sf);
    return ok;
}

} // namespace corncob::game
