// Inst.cpp - Port of INST.ASM (the vgmode 10h, non-slalom build)
//
// Gauge records in DGROUP (altxc, aspxc ...): +0 center x, +2 center y, +4 x size, +6 y size,
// +8 where the needle ended on page 0, +12 on page 1, +16 color. The ASM's 16-bit arithmetic
// is kept (the rounding of the needle ends in particular).

#include "Inst.hpp"
#include "DGroup.hpp"
#include "core/AsmMath.hpp"
#include "engine/AsmTables.hpp"
#include <algorithm>
#include <cstdlib>

namespace corncob::game {

namespace d = corncob::dgroup;
namespace lbl = corncob::lbl;
using asmx::bndck;
using asmx::idiv32;
using asmx::imul16;
using asmx::sf16;
using engine::atn2;
using engine::scos;
using engine::ssin;

#define L(name) lbl::name

const u32 Instruments::PGFLG = lbl::pgflg;

namespace {

inline i16 add16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) + static_cast<u16>(b))); }
inline i16 sub16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) - static_cast<u16>(b))); }
inline i16 neg16(i16 a) { return static_cast<i16>(static_cast<u16>(0u - static_cast<u16>(a))); }
inline i16 hi16(int32_t v) { return static_cast<i16>(static_cast<uint32_t>(v) >> 16); }
// "imul dx / shl ax,1 / adc dx,0 / mov ax,dx": the product rounded to its high word
inline i16 scale(i16 value, i16 size) {
    const int32_t p = imul16(value, size);
    const uint32_t lo = static_cast<uint32_t>(p) & 0xFFFF;
    return add16(hi16(p), static_cast<i16>((lo >> 15) & 1));
}

constexpr int THXMIN = 481, THXMAX = 629, THY = 246;
constexpr int NDBARS = 13;

} // namespace

// ---- drawing helpers ----

void Instruments::px(int x, int y, u8 c) {
    const int s = m_scr.scale();
    m_scr.fillRect(x * s, y * s, s, s, c);
}

// EGALINE.ASM: Bresenham with both ends included (a vertical or horizontal line is just a run)
void Instruments::line(int x1, int y1, int x2, int y2, u8 c) {
    if (x2 < x1) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }
    if (x1 == x2) {
        for (int y = std::min(y1, y2); y <= std::max(y1, y2); y++) px(x1, y, c);
        return;
    }
    const int dx = x2 - x1, dy = std::abs(y2 - y1), sy = y2 > y1 ? 1 : -1;
    if (dy == 0) {
        for (int x = x1; x <= x2; x++) px(x, y1, c);
        return;
    }
    if (dy <= dx) {
        int dec = 2 * dy - dx;
        for (int x = x1, y = y1; x <= x2; x++) {
            px(x, y, c);
            if (dec >= 0) { y += sy; dec += 2 * (dy - dx); }
            else dec += 2 * dy;
        }
    } else {
        int dec = 2 * dx - dy;
        for (int y = y1, x = x1, n = 0; n <= dy; n++, y += sy) {
            px(x, y, c);
            if (dec >= 0) { x++; dec += 2 * (dx - dy); }
            else dec += 2 * dx;
        }
    }
}

// angtocord: the end of a needle of this gauge at this angle
void Instruments::angtocord(i16 angle, u32 g, i16& x2, i16& y2) {
    y2 = add16(scale(neg16(scos(angle)), W(g + 6)), W(g + 2));   // the screen's origin is at the top
    x2 = add16(scale(ssin(angle), W(g + 4)), W(g));
}

// drawneed: a needle from the gauge center to (x2,y2), remembered for the erase two frames later
void Instruments::drawneed(u32 g, i16 x2, i16 y2) {
    const u32 old = g + 8 + pgadd();
    SW(old, x2);
    SW(old + 2, y2);
    line(W(g), W(g + 2), x2, y2, B(g + 16));
}

void Instruments::erasneed(u32 g) {
    const u32 old = g + 8 + pgadd();
    line(W(g), W(g + 2), W(old), W(old + 2), 0);
}

// ---- the needles ----

void Instruments::rlicrd1() {   // the artificial horizon: a line through the center, tilted by the roll
    const u32 g = L(rlixc);
    const i16 ang = W(L(orvec) + 4);
    const i16 dy = scale(ssin(ang), W(g + 6));
    const i16 y2 = add16(dy, W(g + 2)), y1 = sub16(W(g + 2), dy);
    const i16 dx = scale(scos(ang), W(g + 4));
    const i16 x2 = add16(dx, W(g)), x1 = sub16(W(g), dx);
    const u32 old = g + 8 + pgadd();
    SW(old, x2);
    SW(old + 2, y2);
    line(x1, y1, x2, y2, B(g + 16));
}

void Instruments::pitcrd1() {   // the pitch ladder: like the horizon, with the pitch as the angle
    i16 ax = neg16(W(L(orvec) + 2));
    if (sf16(add16(W(L(orvec) + 4), 16384), 0)) ax = add16(neg16(ax), static_cast<i16>(32768));   // upside down
    const u32 g = L(pitxc);
    const i16 dy = scale(ssin(ax), W(g + 6));
    const i16 y2 = add16(dy, W(g + 2)), y1 = sub16(W(g + 2), dy);
    const i16 dx = scale(scos(ax), W(g + 4));
    const i16 x2 = add16(dx, W(g)), x1 = sub16(W(g), dx);
    m_pargx2 = x2;
    m_pargy2 = y2;
    const u32 old = g + 8 + pgadd();
    SW(old, x2);
    SW(old + 2, y2);
    line(x1, y1, x2, y2, B(g + 16));
}

void Instruments::altdraw() {   // the other two altimeter hands: 10 and 100 times slower
    const u32 g = L(altxc);
    const struct { u32 rec; u32 clr; int div; } hands[2] = {{L(altxc2), L(altclr2), 10}, {L(altxc3), L(altclr3), 10}};
    for (const auto& h : hands) {
        m_alt1 /= static_cast<uint64_t>(h.div);
        const i16 ang = static_cast<i16>(m_alt1 & 0xFFFF);
        const i16 y2 = add16(scale(neg16(scos(ang)), W(h.rec + 6)), W(g + 2));
        const i16 x2 = add16(scale(ssin(ang), W(h.rec + 4)), W(g));
        const u32 old = h.rec + 8 + pgadd();
        SW(old, x2);
        SW(old + 2, y2);
        line(W(g), W(g + 2), x2, y2, B(h.clr));
    }
}

void Instruments::drawthrt() {   // a ball at the throttle position
    const u32 num = static_cast<u32>(static_cast<u16>(W(L(rpm)))) * (THXMAX - THXMIN);
    const u16 rpmx = static_cast<u16>(W(L(rpmx)));
    const int xc = static_cast<int>(rpmx ? num / rpmx : 0) + THXMIN - 3;
    SW(L(throld1) + pgadd() / 2, static_cast<i16>(xc));
    const int s = m_scr.scale();
    engine::Win w{};
    w.xmin = static_cast<i16>(THXMIN * s);
    w.ymin = static_cast<i16>((THY - 3) * s);
    w.xmax = static_cast<i16>(THXMAX * s);
    w.ymax = static_cast<i16>((THY + 3) * s);
    SW(L(xc), static_cast<i16>(xc * s));
    SW(L(yc), static_cast<i16>(THY * s));
    SW(L(xw), static_cast<i16>(5 * s));
    m_g.setB(L(n), 8);
    m_eng.drawdisc(w);
}

void Instruments::erasthr() {
    const int s = m_scr.scale();
    engine::Win w{};
    w.xmin = static_cast<i16>(THXMIN * s);
    w.ymin = static_cast<i16>((THY - 3) * s);
    w.xmax = static_cast<i16>(THXMAX * s);
    w.ymax = static_cast<i16>((THY + 3) * s);
    SW(L(xc), static_cast<i16>(W(L(throld1) + pgadd() / 2) * s));
    SW(L(xw), static_cast<i16>(5 * s));
    SW(L(yc), static_cast<i16>(THY * s));
    m_g.setB(L(n), 0);
    m_eng.drawdisc(w);
}

void Instruments::drawmds() {   // the flight path ball: where the velocity points
    i16 ax = neg16(atn2(W(L(topv)), W(L(topv) + 4)));
    ax = add16(ax, 5400);
    if (ax < 0) ax = 0;
    if (!sf16(ax, 10800)) ax = 10799;
    const int yc = static_cast<int>(static_cast<uint32_t>(static_cast<u16>(ax)) * 110 / 10800) - 55 + W(L(mdsyc));
    const u32 old = L(mdsold1) + pgadd();
    SW(old + 2, static_cast<i16>(yc));
    ax = atn2(W(L(topv)), W(L(topv) + 2));
    ax = add16(ax, 5400);
    if (ax < 0) ax = 0;
    if (!sf16(ax, 10800)) ax = 10799;
    const int xc = static_cast<int>(static_cast<uint32_t>(static_cast<u16>(ax)) * 160 / 10800) - 80 + W(L(mdsxc));
    SW(old, static_cast<i16>(xc));
    const int s = m_scr.scale();
    SW(L(xc), static_cast<i16>(xc * s));
    SW(L(yc), static_cast<i16>(yc * s));
    SW(L(xw), static_cast<i16>(6 * s));
    m_g.setB(L(n), B(L(mdsclr1)));
    m_eng.drawdisc(m_eng.win(static_cast<u16>(L(win0))));
    // the crosshair: four dots around the center of the window
    const int cx = W(L(win0) + 16), cy = W(L(win0) + 18);
    px(cx - 2, cy, 7);
    px(cx + 2, cy, 7);
    px(cx, cy - 1, 7);
    px(cx, cy + 2, 7);
}

void Instruments::erasmds() {   // clears the whole band the ball can be in
    const u32 w = L(win0);
    const int x0 = (((W(w) - 1) >> 3) + 1) << 3;
    const int bytes = (W(w + 8) + 1) >> 3;
    const int y0 = W(w + 2) - 4;
    const int rows = W(w + 10) + 4;
    const int s = m_scr.scale();
    const u8 mask = m_scr.planeMask();
    m_scr.setPlaneMask(15);
    m_scr.fillRect(x0 * s, y0 * s, bytes * 8 * s, rows * s, 0);
    m_scr.setPlaneMask(mask);
}

void Instruments::drawndles() {
    if (B(L(instflag)) == 0) return;   // not drawing instruments
    m_scr.setPlaneMask(8);
    // altimeter: the fast hand is the altitude above hmin in units of 100 feet, as a turn
    {
        int32_t alt = m_g.dw(L(oz)) - static_cast<int32_t>(static_cast<u16>(W(L(hmin))));
        if (alt < 0 && hi16(alt) < 0) alt = 0;
        m_alt1 = (static_cast<uint64_t>(static_cast<uint32_t>(alt)) << 16) / 3050;
        i16 x2, y2;
        angtocord(static_cast<i16>(m_alt1 & 0xFFFF), L(altxc), x2, y2);
        drawneed(L(altxc), x2, y2);
    }
    // rate of climb
    {
        i16 dx = add16(W(L(rtopv) + 4), 804);
        if (dx < 0) dx = 0;
        if (!sf16(dx, 1608)) dx = 1607;
        const u16 ang = static_cast<u16>((static_cast<uint32_t>(static_cast<u16>(dx)) << 16) / 2414);
        i16 x2, y2;
        angtocord(static_cast<i16>(ang + 27306), L(clmxc), x2, y2);   // zero is at the lower left
        drawneed(L(clmxc), x2, y2);
    }
    {   // roll indicator needle and the horizon line
        i16 x2, y2;
        angtocord(W(L(orvec) + 4), L(rlixc2), x2, y2);
        drawneed(L(rlixc2), x2, y2);
        rlicrd1();
    }
    // one of the slower gauges per frame, the way the frame number picks
    {
        static const int pick[14] = {3, 3, 8, 8, 5, 5, 1, 1, 8, 8, 5, 5, 1, 1};   // l03 l08 l05 l01
        i16 x2, y2;
        switch (pick[static_cast<u16>(W(L(frmnum))) % 14]) {
        case 1: {   // airspeed
            uint32_t v = static_cast<u16>(W(L(topv)));
            if (UW(L(obj5dbits)) & 0x2000) v <<= 2;   // the super plane
            uint32_t rem = v % 19648;
            if (rem >= 19648) rem -= 19648;
            const i16 ang = static_cast<i16>(static_cast<u16>((rem << 16) / 19648));
            angtocord(ang, L(aspxc), x2, y2);
            drawneed(L(aspxc), x2, y2);
            break;
        }
        case 3: {   // engine temperature
            const i16 ang = sub16(static_cast<i16>(static_cast<u16>(W(L(etemp))) / 3), 5400);
            angtocord(ang, L(tmpxc), x2, y2);
            drawneed(L(tmpxc), x2, y2);
            break;
        }
        case 5: {   // compass
            angtocord(neg16(W(L(orvec))), L(cmpxc), x2, y2);
            drawneed(L(cmpxc), x2, y2);
            break;
        }
        default: {  // the pitch ladder and its second needle at the end of it
            pitcrd1();
            i16 ax = neg16(W(L(orvec) + 2));
            if (sf16(add16(W(L(orvec) + 4), 16384), 0)) ax = add16(neg16(ax), static_cast<i16>(32768));
            SW(L(pitxc2), m_pargx2);   // the center is at the end of the ladder line
            SW(L(pitxc2) + 2, m_pargy2);
            angtocord(ax, L(pitxc2), x2, y2);
            drawneed(L(pitxc2), x2, y2);
            break;
        }
        }
    }
    drawthrt();
    m_scr.setPlaneMask(15);
    // the oil pressure and the other two altimeter hands use all planes
    {
        const u16 q = static_cast<u16>(static_cast<u16>(W(L(opres))) / 3);
        const i16 ang = sub16(static_cast<i16>(static_cast<u16>(q << 2)), 32767 - 5400);
        i16 x2, y2;
        angtocord(ang, L(oprxc), x2, y2);
        drawneed(L(oprxc), x2, y2);
    }
    altdraw();
    drawmds();
}

void Instruments::erasndles() {
    if (B(L(instflag)) == 0) return;
    m_scr.setPlaneMask(8);
    erasneed(L(altxc));
    erasneed(L(clmxc));
    erasneed(L(rlixc2));
    erasrli();
    {
        static const int pick[14] = {3, 3, 8, 8, 5, 5, 1, 1, 8, 8, 5, 5, 1, 1};
        switch (pick[static_cast<u16>(W(L(frmnum))) % 14]) {
        case 1: erasneed(L(aspxc)); break;
        case 3: erasneed(L(tmpxc)); break;
        case 5: erasneed(L(cmpxc)); break;
        default: {
            // eraspit: the ladder line, with the end mirrored; eraspit2: the line from its end to the second needle
            const u32 g = L(pitxc);
            const u32 old = g + 8 + pgadd();
            const i16 x2 = W(old), y2 = W(old + 2);
            line(sub16(add16(W(g), W(g)), x2), sub16(add16(W(g + 2), W(g + 2)), y2), x2, y2, 0);
            const u32 g2 = L(pitxc2);
            const u32 old2 = g2 + 8 + pgadd();
            line(x2, y2, W(old2), W(old2 + 2), 0);
            break;
        }
        }
    }
    erasthr();
    m_scr.setPlaneMask(15);
    erasneed(L(altxc2));
    erasneed(L(altxc3));
    erasneed(L(oprxc));
    erasmds();
}

void Instruments::erasrli() {
    const u32 g = L(rlixc);
    const u32 old = g + 8 + pgadd();
    const i16 x2 = W(old), y2 = W(old + 2);
    line(sub16(add16(W(g), W(g)), x2), sub16(add16(W(g + 2), W(g + 2)), y2), x2, y2, 0);
}

// ---- buttons and damage bars ----

void Instruments::drawbutton(u16 ax, int bx) {
    if (W(UW(L(ptrgwinf)) + 6) >= 210) return;   // not with the full window up
    static const int unused = 0;
    (void)unused;
    const u32 rec = L(autodata) + 8u * static_cast<u32>(bx);
    m_scr.replaceColor(W(rec), W(rec + 2), W(rec + 4), W(rec + 6), static_cast<u8>(ax & 0xFF), static_cast<u8>(ax >> 8));
}

int Instruments::damindicinit(int bx) {
    const u32 si = L(edata) + 10u * static_cast<u32>(bx);
    const int cx = W(si + 4);   // the damage shown so far
    SW(si + 4, 0);
    SW(si + 6, 0);
    SW(si + 8, 0);
    return cx;
}

void Instruments::drawdamage(int bx) {
    if (W(UW(L(ptrgwinf)) + 6) >= 210) return;
    const u32 si = L(edata) + 10u * static_cast<u32>(bx);
    if (W(si + 8) != 0) {   // flashing already: just flash faster
        if (!(UW(si + 8) < 256)) SW(si + 8, static_cast<i16>(UW(si + 8) >> 1));
        return;
    }
    // dosolid: draw the next segment
    const int count = W(si + 4);
    const u8 color = B(L(clrseq) + count);
    const int x1 = count * 6 + W(si);
    const int y = W(si + 2);
    m_scr.fillRectBoth(x1 * m_scr.scale(), y * m_scr.scale(), 6 * m_scr.scale(), m_scr.scale(), color);
    SW(si + 4, static_cast<i16>(count + 1));
    if (count + 1 > NDBARS - 1) {   // start the flashing
        SW(si + 4, NDBARS - 1);
        SW(si + 6, static_cast<i16>(m_clock.oticks));   // our own sync, so they don't flash in unison
        SW(si + 8, 512);                                // at the lowest rate
    }
}

void Instruments::flashdamage(int bx) {
    if (W(UW(L(ptrgwinf)) + 6) >= 210) return;
    const u32 si = L(edata) + 10u * static_cast<u32>(bx);
    if (W(si + 8) == 0) return;   // solid
    const u16 dx = static_cast<u16>(m_clock.oticks - UW(si + 6));
    const u8 color = (dx & UW(si + 8)) ? 0x0E : 0x0C;   // yellow or red
    const int count = W(si + 4);
    const int x1 = count * 6 + W(si);
    const int y = W(si + 2);
    const int s = m_scr.scale();
    m_scr.fillRectBoth(x1 * s, y * s, 6 * s, s, color);
    SW(si + 4, static_cast<i16>(count + 1));
    if (count + 1 > NDBARS - 1) SW(si + 4, NDBARS - 1);   // strtdflash: already flashing
}

// ---- tick marks ----

void Instruments::clearticks() {
    const int s = m_scr.scale();
    m_scr.setPlaneMask(8);
    for (int y = 212 * s; y < 237 * s; y++) m_scr.hspan(360 * s, 431 * s, y, 0);
    m_scr.setPlaneMask(15);
}

void Instruments::tickline(int x1, int y1, int x2, int y2) {
    m_scr.setPlaneMask(8);
    line(x1, y1, x2, y2, 8);
    m_scr.setPlaneMask(15);
}

void Instruments::drawelvtick(i16 ax) {
    ax = add16(neg16(ax), 0x200);
    const int y = idiv32(imul16(ax, 237 - 212), 0x400).q + 212;
    tickline(368 - 3, y, 368 + 3, y);
}

void Instruments::drawrdrtick(i16 ax) {
    ax = add16(neg16(ax), 0x200);
    const int x = idiv32(imul16(ax, 429 - 385), 0x400).q + 385;
    tickline(x, 216 - 2, x, 216 + 2);
}

void Instruments::drawairtick(i16 ax) {
    ax = add16(ax, 0x200);
    const int x = idiv32(imul16(ax, 429 - 385), 0x400).q + 385;
    tickline(x, 232 - 2, x, 232 + 2);
}

// ---- the gunsight ----

void Instruments::drawgs() {
    const u16 win = UW(L(ptrgwinf));
    const int cx = W(win + 16), cy = W(win + 18);
    constexpr int gsz = 12, gsz23 = 8, gsz2323 = 5;
    const u8 c = 7;   // the gunsight is grey
    // left and right arms: two lines thick
    int x1 = cx - gsz;
    line(x1, cy, x1 + gsz23, cy, c);
    line(x1, cy + 1, x1 + gsz23, cy + 1, c);
    int xr = x1 + gsz + gsz + 1;
    line(xr, cy, xr - gsz23, cy, c);
    line(xr, cy + 1, xr - gsz23, cy + 1, c);
    // top and bottom
    const int y1 = cy - gsz23;
    line(cx, y1, cx, y1 + gsz2323, c);
    line(cx + 1, y1, cx + 1, y1 + gsz2323, c);
    const int yb = y1 + gsz23 + gsz23 + 1;
    line(cx, yb - gsz2323, cx, yb, c);
    line(cx + 1, yb - gsz2323, cx + 1, yb, c);
}

} // namespace corncob::game
