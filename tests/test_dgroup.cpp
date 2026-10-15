// test_dgroup.cpp - Validates the generated DGROUP image against hand-checked data
// (the player plane transcribed in src/game/PlayerObjects.cpp from DEFOBJS.ASM),
// plus GameState accessors and the TickClock rates.

#include "DGroup.hpp"
#include "game/GameState.hpp"
#include "game/TickClock.hpp"
#include <cstdio>
#include <cstring>

using namespace corncob;
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

static uint32_t sym(const char* module, const char* name) {
    for (size_t i = 0; i < d::kSymbolCount; i++)
        if (!std::strcmp(d::kSymbols[i].module, module) && !std::strcmp(d::kSymbols[i].name, name))
            return d::kSymbols[i].offset;
    std::printf("missing symbol %s:%s\n", module, name);
    failures++;
    return 0;
}

static bool hasCodeRef(uint32_t off, const char* name) {
    for (size_t i = 0; i < d::kCodeRefCount; i++)
        if (d::kCodeRefs[i].offset == off) return !std::strcmp(d::kCodeRefs[i].name, name);
    return false;
}

int main() {
    GameState gs;
    CHECK_EQ(d::kImageSize < 0x10000, 1);

    // DEFOBJS constants: lp=180, lpo2=120, lp2=360, ldip=360, l8=1440
    const int16_t lp = 180, lpo2 = 120, lp2 = 360, ldip = 360, l8 = 1440;

    // plnplyw2: dw 4 / dw wingclr / 4 vertices
    uint32_t w2 = d::defobjs::plnplyw2;
    CHECK_EQ(gs.w(w2), 4);
    CHECK_EQ(gs.uw(w2 + 2), d::defobjs::wingclr); // indirect colour = address of a colour byte
    CHECK_EQ(gs.b(d::defobjs::wingclr), 1);
    const int16_t w2v[12] = {lp2, lpo2, 0, -lp2, lpo2, 0, -lp, ldip, -lpo2, lp2, ldip, -lpo2};
    for (int i = 0; i < 12; i++) CHECK_EQ(gs.w(w2 + 4 + i * 2), w2v[i]);

    // plnplyw2tip ends at the wing tip (lp2, l8, 0)
    uint32_t tip = d::defobjs::plnplyw2tip;
    CHECK_EQ(gs.w(tip + 4 + 9 * 2), lp2);
    CHECK_EQ(gs.w(tip + 4 + 10 * 2), l8);

    // plnply2: the original's l4 (=240, from F3DEQU.INC) in the third vertex
    uint32_t p2 = d::defobjs::plnply2;
    CHECK_EQ(gs.w(p2 + 4 + 6 * 2), 240);

    // plnplyobj header: 0,0,0 then 23 parts per view, first part = (8, plnplyw2)
    uint32_t obj = d::defobjs::plnplyobj;
    CHECK_EQ(gs.w(obj), 0);
    CHECK_EQ(gs.w(obj + 6), 23);
    CHECK_EQ(gs.w(obj + 8), 8);
    CHECK_EQ(gs.uw(obj + 10), d::defobjs::plnplyw2);

    // propobj uses rept 8 to build its 8 view lists of 13 parts
    uint32_t prop = d::defobjs::propobj;
    CHECK_EQ(gs.w(prop + 6), 13);
    CHECK_EQ(gs.uw(prop + 8 + 13 * 4 * 7 + 2), d::defobjs::propply1tip); // view 8, first part

    // ADL.ASM: tables padded to 28 bytes with (28+tbl-$) dup(-1), then a -1 terminator
    CHECK_EQ(d::adl::guntbl - d::adl::enginetbl, 29);

    // Cross-module symbol table works and 3.ASM data has its initial values
    CHECK_EQ(sym("3.ASM", "missionstatus"), d::m3::missionstatus);
    CHECK_EQ(gs.w(d::m3::missionstatus), 1);
    CHECK_EQ(gs.w(d::m3::maxofrm), 35);
    CHECK_EQ(gs.w(d::m3::rtopv), 100);

    // Code pointers inside data are recorded by name (the part-type dispatch is
    // somewhere in F3DVEC; at least one data word must refer to drwply)
    bool sawDrwply = false;
    for (size_t i = 0; i < d::kCodeRefCount; i++)
        if (!std::strcmp(d::kCodeRefs[i].name, "drwply")) sawDrwply = true;
    CHECK_EQ(sawDrwply, 1);
    (void)hasCodeRef;

    // GameState 32-bit pairs are little-endian lo,hi
    gs.setDw(d::m3::ox, 0x12345678);
    CHECK_EQ(gs.uw(d::m3::ox), 0x5678);
    CHECK_EQ(gs.uw(d::m3::ox + 2), 0x1234);
    gs.reset();
    CHECK_EQ(gs.w(d::m3::missionstatus), 1);

    // TickClock: ~1165 fast ticks and ~18 slow ticks per second
    TickClock clk;
    int slow = clk.advance(1.0);
    CHECK_EQ(clk.oticks, 1165);
    CHECK_EQ(slow, 1165 / 64);
    CHECK_EQ(clk.ticks, 18);

    if (failures == 0) std::printf("test_dgroup: all passed (%zu bytes, %zu symbols)\n", d::kImageSize, d::kSymbolCount);
    return failures == 0 ? 0 : 1;
}
