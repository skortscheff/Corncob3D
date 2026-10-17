// Inst.hpp - INST.ASM: the needle gauges, buttons, damage bars, control ticks and the gunsight
//
// Everything is drawn into the EGA pages the way the original does it, only the plane tricks
// are done with EgaScreen's plane mask:
//   * needles are drawn in color 8 into bit plane 3 only (map mask 8), so a needle crossing the
//     numerals of a gauge makes them white and erasing it (color 0, plane 3) restores them;
//   * buttons change colors in place (drawbutton replaces one color by another);
//   * each gauge remembers where its needle was on each of the two pages (pgflg) and erases it
//     two frames later, and one of the slower gauges is drawn per frame (frmnum mod 14).
// Coordinates are the 640x350 ones; HD mode draws every pixel as a scale x scale block.

#pragma once
#include "engine/F3dVec.hpp"
#include "game/GameState.hpp"
#include "game/TickClock.hpp"
#include "render/EgaScreen.hpp"
#include <cstdint>

namespace corncob::game {

using engine::i16;
using engine::u16;
using engine::u32;
using engine::u8;

class Instruments {
public:
    Instruments(GameState& gs, engine::F3dVec& eng, EgaScreen& scr, const TickClock& clock)
        : m_g(gs), m_eng(eng), m_scr(scr), m_clock(clock) {}

    // The needles (drawndles) and the erasing of the ones drawn two frames ago (erasndles)
    void drawndles();
    void erasndles();

    // ax = (new color << 8) | search color: replaces the search color by the new one inside
    // the button bx (0 auto, 1 hold, 2 flaps, 3 stall, 4 eject, 5 complete, 6-8 bombs, 9 low
    // altitude, 10-11 the ten thousands, 12 help, 13 the damage box)
    void drawbutton(u16 ax, int bx);

    // The four damage bars (0 engine, 1 wing, 2 stability, 3 rudder)
    void drawdamage(int bx);
    void flashdamage(int bx);
    int damindicinit(int bx);   // returns the damage shown so far and clears the bar

    // The tick marks that show the control positions
    void clearticks();
    void drawairtick(i16 ax);
    void drawelvtick(i16 ax);
    void drawrdrtick(i16 ax);

    // The gunsight in the middle of the window
    void drawgs();

private:
    i16 W(u32 o) const { return m_g.w(o); }
    u16 UW(u32 o) const { return m_g.uw(o); }
    void SW(u32 o, i16 v) { m_g.setW(o, v); }
    u8 B(u32 o) const { return m_g.b(o); }

    void px(int x, int y, u8 c);
    void line(int x1, int y1, int x2, int y2, u8 c);   // egaline: both ends included
    void angtocord(i16 angle, u32 gauge, i16& x2, i16& y2);
    void drawneed(u32 gauge, i16 x2, i16 y2);
    void erasneed(u32 gauge);
    void erasrli();
    void eraspit2();
    void drawthrt();
    void erasthr();
    void drawmds();
    void erasmds();
    void rlicrd1();
    void pitcrd1();
    void altdraw();
    void tickline(int x1, int y1, int x2, int y2);
    int pgadd() const { return B(PGFLG) ? 4 : 0; }

    GameState& m_g;
    engine::F3dVec& m_eng;
    EgaScreen& m_scr;
    const TickClock& m_clock;

    // the altitude needles are the same quantity divided by 10 and 100
    uint64_t m_alt1 = 0;
    i16 m_pargx2 = 0, m_pargy2 = 0;

    static const u32 PGFLG;
};

} // namespace corncob::game
