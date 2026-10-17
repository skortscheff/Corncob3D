// F3dVec.hpp - The 3D engine: F3DVEC.ASM, MAT.ASM, HLINE.ASM, KLINE/EGALINE.ASM
//
// Each method is the ASM proc of the same name. Pointers (si, bx, di, dx) are
// DGROUP offsets into GameState, so object shapes, matrices and scratch buffers
// are the original data and keep the original's aliasing (e.g. relrot writes
// the rotated object origin into the first three words of the shape, which
// drrobj then reads back as orgptr). Proc-private scalars live here as members.
//
// Vectors handed to the projection code are (depth, x, y): [si] is the distance
// in front of the viewer, screen x = xctr - x*zxpln/depth, y = yctr - y*zypln/depth.
//
// Rendering goes into the back EgaScreen page. HD mode multiplies every window
// constant by the screen scale; the arithmetic stays 16-bit.
//
// Not yet ported (later phases): g3dobj/srel3d/grel3d (ground dots, Phase 3),
// relobj5d, the tile/memseg code (Phase 2), qtimed/qcollis/qcrash/qwalk and
// slam (Phases 3 and 5, stubbed in drawnear), getang (needs ATN.ASM atn2).

#pragma once
#include "game/GameState.hpp"
#include "render/EgaScreen.hpp"
#include <cstdint>
#include <vector>

namespace corncob::engine {

using i16 = int16_t;
using u16 = uint16_t;
using u8 = uint8_t;
using i32 = int32_t;
using u32 = uint32_t;

// A window record (F3DEQU.INC wxmin..ymaxdi) with HD scaling applied
struct Win {
    i16 xmin, ymin, xmax, ymax, xsiz, ysiz, zxpln, zypln, xctr, yctr;
    i16 ymindi, ymaxdi;   // raw byte addresses, as stored
    int minRow, maxRow;   // rows rawhlinewin accepts: [minRow, maxRow)
    int fillRow, fillX;   // where scrfbx starts filling (ymindi as a row/pixel)
};

// What the engine needs from the game code: the per-type object routines (DEFOBJS.ASM
// objtimtable, called with si = object, di = the other object or 0xFFFF for the plane,
// carry = "destroy") and 3.ASM's damageplane.
class GameHooks {
public:
    virtual ~GameHooks() = default;
    virtual void objroutine(u16 type, u16 si, u16 di, bool carry) = 0;
    virtual void damageplane(u16 count) = 0;
    virtual void crashSound(bool boom) { (void)boom; }   // qcrash: a blam, or a boom when out of the plane
};

class F3dVec {
public:
    F3dVec(GameState& gs, EgaScreen& scr);

    void setHooks(GameHooks* h) { m_hooks = h; }
    // The timer's oticks (cs:oticks), which qcrash reads for the wall damage interval
    void setTickSource(const u16* oticks) { m_clockTicks = oticks; }

    // ---- matrices (F3DVEC.ASM, MAT.ASM) ----
    void calcsin(u16 bx);                 // angles yaw,pitch,roll at [bx]
    void negsin();
    void calcmat(u16 si);
    void ncalcmat(u16 si);
    void dot(u16 si, u16 bx, u16 di);     // hmul/madd dot product
    void dot2(u16 si, u16 bx, u16 di);    // column [si],[si+6],[si+12] . row [bx]
    void primdot(u16 si, u16 bx, u16 di); // fast unclamped dot
    void matvmul(u16 si, u16 bx, u16 di);
    void mat2mul(u16 si, u16 bx, u16 di);
    void pmvmul(u16 si, u16 bx, u16 di);
    void matmul();                        // omat = orot * tmat
    void rolpln(i16 roll, i16 pitch, i16 yaw, u16 si); // MAT.ASM: [si] = [si] * rot(angles)

    // ---- projection and primitives ----
    Win win(u16 off) const;
    bool f3dpt(u16 si, u16 bx);
    bool f3ddsc(u16 si, u16 bx);
    bool f3dline(u16 si, u16 bx);
    bool f3dpoly(u16 si, u16 bx);
    bool drawpt(const Win& w);
    bool drawdisc(const Win& w);
    void drawpoly(const Win& w, u16 bx);
    bool hline(const Win& w);             // HLINE.ASM: horizon line + sky/ground fill
    void kline();                         // KLINE.ASM/EGALINE.ASM line in color n

    // ---- objects ----
    bool drrobj(u16 si, u16 bx, u16 dx);
    bool rel3d(u16 si);
    bool disteval(u16 si);
    void d3dobj(u16 si, u16 dx);
    void drawdirect(u16 si, u16 dx);
    void drawrdirect(u16 si, u16 dx);
    void relrot(u16 si);
    void nrelrot(u16 si);
    void gnrlrot(u16 si);
    void modhr();
    void tbadd(u16 si, u16 bx);
    void tbsort(u16 si);
    void tbrmv(u16 si, u16 bx);
    void drawnear();

    // ---- page, borders, palette ----
    void epage();
    void flipage();
    void readcolors();
    void unpackbits();
    void packbits();
    void setskygnd();
    void setcolors(u8 mul);
    void distcolors();   // hallucination: shift each channel by haluclrs, bouncing at 63
    const EgaPalette& palette() const { return m_pal; }
    int panX() const { return m_panX; }
    int panY() const { return m_panY; }

    // cs:cecode, the original's panic/error code (nonzero = something went wrong)
    u8 cecode = 0;

private:
    // GameState access
    i16 W(u32 o) const { return m_g.w(o); }
    void SW(u32 o, i16 v) { m_g.setW(o, v); }
    u8 B(u32 o) const { return m_g.b(o); }
    void SB(u32 o, u8 v) { m_g.setB(o, v); }
    void movsw(u16 di, u16 si, int words);

    // F3DVEC internals
    bool choplin(u16 si, u16 bx);
    bool polyhell();
    void rawhlinewin(const Win& w, int row);
    bool ladvance(u16 si);
    bool radvance(u16 si);
    bool oclip(const Win& w);
    void fixx1a(const Win& w);
    void fixx1b(const Win& w);
    void fixx2a(const Win& w);
    void fixx2b(const Win& w);
    void scode1(const Win& w);
    void scode2(const Win& w);
    void csetup();
    i16 yline1(i16 ax);
    i16 xline(i16 ax);
    void projectVertex(u16 si, u16 bx, u16 dst); // drwire/drpoly inner rotate+translate
    void viewln(u16 si);
    void viewpoly(u16 si);
    void viewpt(u16 si);
    void viewdsc(u16 si);
    void xvect(u16 si, u16 bx);
    void drwire(u16 si, u16 bx);
    void drpoly(u16 si, u16 bx);
    void drawrcld(u16 si, u16 bx);
    void drawrdsc(u16 si, u16 bx);
    void drrfrm(u16 si, u16 bx);
    u8 colorOf(u16 word) const;
    void scrfbx(int row, int x, int rows, int words, u8 color);
    bool checkforrunway(u16 si);
    void reducecoords();
    u16 rand_w();

    // ---- F3DEVC.ASM object interaction (F3dObj.cpp) ----
public:
    u16 vmag(u16 si);                       // magnitude of the vector at [si]
    bool relobj5d(u16 si);                  // like rel3d, relative to the player's plane (obj5d)
    void qtimed(u16 si);                    // runs the object's timed routine
    void qcollis(u16 si, u16 bx);           // bx = si's slot in the table
    void qwalk(u16 si);                     // walking person vs object
    void qcrash(u16 si);                    // plane vs object
    void slam(u16 si, u16 di);              // two objects collided
    u16 rangeBits() const { return m_range; }   // cx left by rel3d / relobj5d

    // Ground detail points (the gscum far segment of 3.ASM): ngpts, then the gpts, fgpts and
    // sgpts arrays of 14 byte records (dword x, y, z and a flag word)
    static constexpr int GNPTS = 80;
    static constexpr u32 GPTS = 2;                                  // offset of gpts
    static constexpr u32 FGPTS = GPTS + (2 + GNPTS) * 14;
    static constexpr u32 SGPTS = FGPTS + (2 + GNPTS) * 14;
    std::vector<uint8_t>& gscum() { return m_gscum; }
    bool g3dobj(u32 point, bool star);     // draws a ground dot / star; carry = too far, redo it
    void eraseWin(u16 f);                  // 3.ASM erasewinf: window f painted black
    void setRdf(u8 v) { m_rdf = v; }
    u8 rdf() const { return m_rdf; }

private:
    void oldqcollis(u16 si, u16 di, bool& carry);
    void qcollis1(u16 si, u16 di);
    bool getx1(u16 si, u16 di);
    bool qvelon(u16 si, u16 di);
    void divs_qd(u16 di, i32 den);
    void callRoutine(u16 si, u16 di, bool carry);

    void srel3d(u32 point);
    void grel3d(u32 point);
    i32 gsDw(u32 off) const;
    GameHooks* m_hooks = nullptr;
    const u16* m_clockTicks = nullptr;
    std::vector<uint8_t> m_gscum = std::vector<uint8_t>(SGPTS + (2 + 2 * GNPTS) * 14 + 16, 0);
    GameState& m_g;
    EgaScreen& m_scr;
    EgaPalette m_pal;
    int m_panX = 0, m_panY = 0;

    // calcsin results (F3DVEC's own sya..cra)
    i16 m_sya = 0, m_cya = 0, m_spa = 0, m_cpa = 0, m_sra = 0, m_cra = 0;
    u8 m_rdf = 0, m_objfail = 0, m_linefail = 0, m_dvflg = 0;
    u8 m_color = 0;      // the EGA set/reset color the fill routines write
    u16 m_range = 0;     // rel3d's out-of-range bits (cx in the ASM)
    u8 m_code1 = 0, m_code2 = 0;
    i16 m_delx = 0, m_dely = 0;
    u16 m_cwin = 0, m_objptr = 0, m_orgptr = 0, m_lineptr = 0;
    // f3dpoly / polyhell / drawpoly
    u16 m_polywin = 0, m_polyptr = 0, m_lindex = 0, m_rindex = 0, m_nside4 = 0, m_nside6 = 0;
    i16 m_pxmin = 0, m_pymin = 0, m_pxmax = 0, m_pymax = 0, m_pzypln = 0, m_pzcount = 0;
    i16 m_xl1 = 0, m_yl1 = 0, m_xl2 = 0, m_yl2 = 0, m_xr1 = 0, m_yr1 = 0, m_xr2 = 0, m_yr2 = 0;
    i32 m_ldeltax = 0, m_rdeltax = 0;
    u16 m_argx1low = 0, m_argx2low = 0;
    i16 m_polyymin = 0, m_polyymax = 0;
    u8 m_hlinemask = 0xFF;
    // tables
    u16 m_tbstrt = 0, m_odcode = 0, m_objcount = 0;
    u16 m_rand = 0x7777;
};

} // namespace corncob::engine
