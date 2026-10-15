// test_engine.cpp - Tests for the ported 3D engine (F3DVEC/MAT/HLINE, SIN.ASM)
// Expected values are worked out by hand from the 8086 instruction semantics.

#define SDL_MAIN_HANDLED   // plain console main, no SDL2main
#include "DGroup.hpp"
#include "engine/AsmTables.hpp"
#include "engine/F3dVec.hpp"
#include "game/GameState.hpp"
#include "render/EgaScreen.hpp"
#include <cstdio>

using namespace corncob;
using namespace corncob::engine;
namespace d = corncob::dgroup;

static int failures = 0;
#define CHECK_EQ(actual, expected)                                                        \
    do {                                                                                  \
        long long a_ = (actual), e_ = (expected);                                         \
        if (a_ != e_) {                                                                   \
            std::printf("%s:%d: %s = %lld, expected %lld\n", __FILE__, __LINE__, #actual, \
                        a_, e_);                                                          \
            failures++;                                                                   \
        }                                                                                 \
    } while (0)

static uint8_t px(const EgaScreen& s, int x, int y) {
    // the engine draws into the back page
    return const_cast<EgaScreen&>(s).back()[y * s.width() + x];
}

int main(int, char**) {
    // ---- SIN.ASM ----
    CHECK_EQ(ssin(0), 0);
    CHECK_EQ(ssin(16384), 32767);     // table end 65535 >> 1
    CHECK_EQ(ssin(8192), 23170);      // sintbl[128] = 46341 >> 1
    CHECK_EQ(ssin(-8192), -23170);
    CHECK_EQ(ssin(-32768), 0);        // the "test ax,7fffh" shortcut
    CHECK_EQ(scos(0), 32767);
    CHECK_EQ(scos(16384), 0);         // cos 90 = ssin(-32768)
    CHECK_EQ(ssin(100), 314);         // 402 + 402*36/64 = 628, >> 1
    CHECK_EQ(ssin(32767 - 99), 314 + 0); // folded: 32768-32668 = 100
    CHECK_EQ(csqrt(-1), 0);
    CHECK_EQ(csqrt(1024), 0);
    CHECK_EQ(csqrt(0), 1474);

    GameState gs;
    EgaScreen scr(1);
    F3dVec eng(gs, scr);
    const uint16_t m = static_cast<uint16_t>(d::f3dvec::tmat);
    const uint16_t ang = static_cast<uint16_t>(d::mat::rolvec);

    // ---- calcmat / ncalcmat ----
    gs.setW(ang, 0); gs.setW(ang + 2, 0); gs.setW(ang + 4, 0);
    eng.calcsin(ang);
    eng.calcmat(m);
    CHECK_EQ(gs.w(m), 32766);         // hmul(32767, 32767)
    CHECK_EQ(gs.w(m + 2), 0);
    CHECK_EQ(gs.w(m + 4), 0);
    CHECK_EQ(gs.w(m + 8), 32766);     // madd(0, 32766): 16383 clamps to itself, rol
    CHECK_EQ(gs.w(m + 16), 32766);

    // yaw 90: ncalcmat negates the sines first
    gs.setW(ang, 16384);
    eng.calcsin(ang);
    eng.ncalcmat(m);
    CHECK_EQ(gs.w(m), 0);             // cya * cpa, cya = 0
    CHECK_EQ(gs.w(m + 2), -32765);    // madd(0, -32767): -16384 clamps to -16383, rol(0xC001)
    CHECK_EQ(gs.w(m + 6), 32767);     // -hmul(-32767, 32767); product 0xC000FFFF << 1 -> 0x8001
    CHECK_EQ(gs.w(m + 12), 0);        // spa

    // ---- f3dpt projection (forward window: center 319,101, planes 300/200) ----
    const uint16_t gwinf = static_cast<uint16_t>(d::m3::gwinf);
    const uint16_t v = static_cast<uint16_t>(d::f3dvec::drwvec);
    gs.setB(d::m3::n, 15);
    gs.setW(v, 1000); gs.setW(v + 2, 100); gs.setW(v + 4, -50);
    CHECK_EQ(eng.f3dpt(v, gwinf), 0);
    CHECK_EQ(gs.w(d::m3::argx1), 319 - 30);   // -100*300/1000
    CHECK_EQ(gs.w(d::m3::argy1), 101 + 10);   //  50*200/1000
    CHECK_EQ(px(scr, 289, 111), 15);
    gs.setW(v, 300);                          // not beyond the x plane: fails
    CHECK_EQ(eng.f3dpt(v, gwinf), 1);

    // ---- f3dline: near-plane chop then projection ----
    gs.setB(d::m3::hrflg, 0);
    gs.setW(v, 100); gs.setW(v + 2, 0); gs.setW(v + 4, 0);
    gs.setW(v + 6, 500); gs.setW(v + 8, 400); gs.setW(v + 10, 0);
    CHECK_EQ(eng.f3dline(v, gwinf), 0);
    CHECK_EQ(gs.w(v), 301);                   // moved to zxpln + 1
    CHECK_EQ(gs.w(v + 2), 200);               // x0 + 400*200/400
    CHECK_EQ(gs.w(d::m3::argx1), 319 - 199);  // idiv truncates -60000/301
    CHECK_EQ(gs.w(d::m3::argx2), 319 - 240);  // -400*300/500
    // both endpoints behind the plane: rejected
    gs.setW(v, 100); gs.setW(v + 6, 200);
    CHECK_EQ(eng.f3dline(v, gwinf), 1);

    // ---- drawpoly: a 100x30 rectangle in color 4 ----
    const uint16_t tp = static_cast<uint16_t>(d::f3dvec::testpoly);
    const int16_t pts[8] = {100, 50, 200, 50, 200, 80, 100, 80};
    for (int i = 0; i < 8; i++) gs.setW(tp + 2 * i, pts[i]);
    gs.setW(d::f3dvec::nsides, 4);
    gs.setB(d::m3::n, 4);
    eng.drawpoly(eng.win(gwinf), tp);
    CHECK_EQ(px(scr, 150, 60), 4);
    CHECK_EQ(px(scr, 100, 60), 4);    // left edge inclusive
    CHECK_EQ(px(scr, 200, 60), 0);    // right edge exclusive (rawhlinewin [x1,x2))
    CHECK_EQ(px(scr, 150, 49), 0);
    CHECK_EQ(px(scr, 150, 81), 0);

    // ---- hline: level horizon across the window ----
    gs.setW(d::m3::argx1, 8);   gs.setW(d::m3::argy1, 101);
    gs.setW(d::m3::argx2, 631); gs.setW(d::m3::argy2, 101);
    CHECK_EQ(eng.hline(eng.win(gwinf)), 0);
    CHECK_EQ(px(scr, 300, 30), gs.b(d::hline::clrup));    // sky above
    CHECK_EQ(px(scr, 300, 150), gs.b(d::hline::clrdn));   // ground below
    CHECK_EQ(px(scr, 4, 150), 0);                          // left border untouched

    // ---- object tables: far first ----
    const uint16_t tbl = static_cast<uint16_t>(d::f3dvec::neartbl);
    const uint16_t ex = static_cast<uint16_t>(d::strtexp::exps);
    const uint32_t dist[3] = {500, 90000, 7000};
    for (int i = 0; i < 3; i++) {
        uint16_t o = static_cast<uint16_t>(ex + 74 * i);
        gs.setB(o + 18, 0);
        gs.setDw(o + 20, static_cast<int32_t>(dist[i]));
        eng.tbadd(tbl, o);
    }
    CHECK_EQ(gs.w(tbl), 3);
    CHECK_EQ(gs.uw(tbl + 4), ex + 74);       // 90000
    CHECK_EQ(gs.uw(tbl + 6), ex + 148);      // 7000
    CHECK_EQ(gs.uw(tbl + 8), ex);            // 500
    gs.setDw(ex + 20, 1000000);              // the near one moves far away
    eng.tbsort(tbl);
    CHECK_EQ(gs.uw(tbl + 4), ex);
    eng.tbrmv(tbl, static_cast<uint16_t>(ex + 74));
    CHECK_EQ(gs.w(tbl), 2);
    CHECK_EQ(gs.uw(tbl + 6), ex + 148);
    CHECK_EQ(gs.b(ex + 74 + 18), 0xFF);      // flagged as removed
    CHECK_EQ(eng.cecode, 0);

    if (failures) std::printf("%d failure(s)\n", failures);
    else std::printf("all engine tests passed\n");
    return failures ? 1 : 0;
}
