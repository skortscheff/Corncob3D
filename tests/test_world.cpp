// test_world.cpp - Tests for the universe code: theater files, tile encode/decode, memseg
// Needs the original DEF/*.CCT and *.TWR (assets/original, never committed); the
// test is skipped when they are missing.

#define SDL_MAIN_HANDLED
#include "DGroup.hpp"
#include "engine/AsmTables.hpp"
#include "engine/F3dVec.hpp"
#include "game/GameState.hpp"
#include "game/World.hpp"
#include "render/EgaScreen.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef CORNCOB_ASSET_DIR
#define CORNCOB_ASSET_DIR "assets/original"
#endif

using namespace corncob;
using namespace corncob::game;
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

static std::string def(const char* name, const char* ext) {
    return std::string(CORNCOB_ASSET_DIR) + "/DEF/" + name + "." + ext;
}

static bool exists(const std::string& p) {
    std::FILE* f = std::fopen(p.c_str(), "rb");
    if (f) std::fclose(f);
    return f != nullptr;
}

struct Theater {
    const char* name;
    int objects;   // total objects over the 64 tiles
};

int main(int, char**) {
    if (!exists(def("WHTSANDS", "CCT"))) {
        std::printf("test_world: original DEF files not found, skipped\n");
        return 0;
    }
    const Theater theaters[] = {{"WHTSANDS", 51}, {"DUSK", 169}, {"MORNING", 221}, {"WASTELND", 185}};
    for (const Theater& t : theaters) {
        GameState gs;
        EgaScreen scr(1);
        engine::F3dVec eng(gs, scr);
        World w(gs, eng);
        w.cleanexp();
        CHECK_EQ(w.readunivfile(def(t.name, "TWR"), def(t.name, "CCT")), 1);

        // start position: x = -13:3000, y = 0, z = 720 (the eye height of a person)
        CHECK_EQ(gs.dw(d::m3::startcoords), -13 * 65536 + 3000);
        CHECK_EQ(gs.dw(d::m3::startcoords + 4), 0);
        CHECK_EQ(gs.dw(d::m3::startcoords + 8), 720);

        // bring every tile into the med/far table, as dotiles would one by one
        int total = 0;
        for (int y = 0; y < World::NTILESY; y++) {
            for (int x = 0; x < World::NTILESX; x++) {
                gs.setW(d::f3dvec::curtilex, static_cast<int16_t>(x));
                gs.setW(d::f3dvec::curtiley, static_cast<int16_t>(y));
                w.bringmemseg();
                total += gs.w(d::f3dvec::tobjcount);
            }
        }
        CHECK_EQ(total, t.objects);

        int active = 0;
        for (int slot = 0; slot < World::NMAXMEDFAR; slot++) {
            const int off = slot * World::NBYTEXP;
            if (w.memseg()[off + 24] || w.memseg()[off + 25]) active++;
        }
        CHECK_EQ(active, t.objects);
        CHECK_EQ(eng.cecode, 0);

        // farming each tile back must re-encode the very same bytes the file had
        GameState gs2;
        EgaScreen scr2(1);
        engine::F3dVec eng2(gs2, scr2);
        World ref(gs2, eng2);
        ref.readunivfile(def(t.name, "TWR"), def(t.name, "CCT"));
        for (int y = 0; y < World::NTILESY; y++) {
            for (int x = 0; x < World::NTILESX; x++) {
                gs.setW(d::f3dvec::curtilex, static_cast<int16_t>(x));
                gs.setW(d::f3dvec::curtiley, static_cast<int16_t>(y));
                gs2.setW(d::f3dvec::curtilex, static_cast<int16_t>(x));
                gs2.setW(d::f3dvec::curtiley, static_cast<int16_t>(y));
                const size_t base = static_cast<size_t>(w.gettileadr()) * 16;
                // count objects in the reference tile
                int count = 0;
                while (count < World::NOBJTILE - 1 &&
                       (ref.memseg()[base + count * 23 + 11] | ref.memseg()[base + count * 23 + 12]))
                    count++;
                std::vector<uint8_t> before(ref.memseg().begin() + base, ref.memseg().begin() + base + count * 23);
                w.farmmemseg();
                std::vector<uint8_t> after(w.memseg().begin() + base, w.memseg().begin() + base + count * 23);
                if (before != after) {
                    std::printf("%s tile %d,%d: re-encoded tile differs (%d objects)\n", t.name, x, y, count);
                    failures++;
                }
            }
        }
        CHECK_EQ(eng.cecode, 0);
    }

    // ---- towers.dat universe parameters ----
    {
        GameState gs;
        EgaScreen scr(1);
        engine::F3dVec eng(gs, scr);
        World w(gs, eng);
        CHECK_EQ(w.readunivfile(def("DUSK", "TWR"), def("DUSK", "CCT")), 1);
        CHECK_EQ(gs.w(d::m3::sunangle), -15000);
        CHECK_EQ(gs.w(d::defobjs::sunsz1), 200);
        CHECK_EQ(gs.w(d::defobjs::sunsz2), 205);
        CHECK_EQ(gs.b(d::defobjs::sunclr), 12);
        CHECK_EQ(gs.w(d::m3::grvsf), -48);
        CHECK_EQ(gs.w(d::m3::rho0), 6000);
        CHECK_EQ(gs.w(d::m3::rho02), 4000);
        CHECK_EQ(gs.b(d::m3::starsflag), 1);
        CHECK_EQ(gs.w(d::m3::rho0z), 6000);
        CHECK_EQ(gs.w(d::m3::starheight), 3600);
    }
    {
        GameState gs;
        EgaScreen scr(1);
        engine::F3dVec eng(gs, scr);
        World w(gs, eng);
        CHECK_EQ(w.readunivfile(def("MORNING", "TWR"), def("MORNING", "CCT")), 1);
        CHECK_EQ(gs.w(d::m3::sunangle), 14000);
        CHECK_EQ(gs.w(d::defobjs::sunsz1), 150);
        CHECK_EQ(gs.b(d::defobjs::sunclr), 14);
        CHECK_EQ(gs.b(d::m3::starsflag), 0);
    }
    {   // a .TWR without the X line keeps the defaults
        GameState gs, fresh;
        EgaScreen scr(1);
        engine::F3dVec eng(gs, scr);
        World w(gs, eng);
        CHECK_EQ(w.readunivfile(def("WASTELND", "TWR"), def("WASTELND", "CCT")), 1);
        CHECK_EQ(gs.w(d::m3::sunangle), fresh.w(d::m3::sunangle));
        CHECK_EQ(gs.w(d::m3::grvsf), fresh.w(d::m3::grvsf));
    }

    // ---- tile geometry (qintile, updatect) ----
    {
        GameState gs;
        EgaScreen scr(1);
        engine::F3dVec eng(gs, scr);
        World w(gs, eng);
        // universe origin is -128: tile (4,4) spans x,y in [0,32)
        gs.setW(d::f3dvec::curtilex, 4);
        gs.setW(d::f3dvec::curtiley, 4);
        const uint16_t obj = static_cast<uint16_t>(d::strtexp::exps);
        gs.setW(obj + 2, 0);  gs.setW(obj + 6, 31);
        CHECK_EQ(w.qintile(obj), 0);
        gs.setW(obj + 2, 32);
        CHECK_EQ(w.qintile(obj), 1);
        gs.setW(obj + 2, -1);
        CHECK_EQ(w.qintile(obj), 1);
        // the observer in tile (3,4): x hi = -13, y hi = 0
        gs.setDw(d::m3::ox, -13 * 65536 + 3000);
        gs.setDw(d::m3::ox + 4, 0);
        w.updatect();
        CHECK_EQ(gs.w(d::f3dvec::ctx), (-13 + 128 + 16) / 32);   // 131/32 = 4
        CHECK_EQ(gs.w(d::f3dvec::cty), (0 + 128 + 16) / 32);     // 144/32 = 4
    }

    if (failures == 0) std::printf("test_world: all passed\n");
    return failures ? 1 : 0;
}
