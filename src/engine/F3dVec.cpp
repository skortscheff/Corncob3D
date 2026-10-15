// F3dVec.cpp - Port of F3DVEC.ASM (with MAT.ASM, HLINE.ASM, KLINE.ASM, EGALINE.ASM)
//
// Comparisons follow the original's flags: "js/jns" after CMP test the sign of
// the 16-bit difference (sf16), which differs from a signed compare when the
// subtraction overflows; "jl/jg/jle/jge" are real signed compares.

#include "F3dVec.hpp"
#include "AsmTables.hpp"
#include "DGroup.hpp"
#include "core/AsmMath.hpp"
#include <algorithm>
#include <cstdlib>

namespace corncob::engine {

namespace d = corncob::dgroup;
using asmx::hmul;
using asmx::imul16;
using asmx::idiv32;
using asmx::madd;
using asmx::sf16;

namespace {

// 3.ASM globals
constexpr u32 ARGX1 = d::m3::argx1, ARGY1 = d::m3::argy1, ARGX2 = d::m3::argx2, ARGY2 = d::m3::argy2;
constexpr u32 N = d::m3::n;
constexpr u32 OX = d::m3::ox;
constexpr u32 XVEC = d::m3::xvec, DXVEC = d::m3::dxvec;
constexpr u32 PTRGWINF = d::m3::ptrgwinf, PTRGWINT = d::m3::ptrgwint;
constexpr u32 HRFLG = d::m3::hrflg, FILLFLG = d::m3::fillflg;
constexpr u32 HROBJ = d::m3::hrobj, HRWR1 = d::m3::hrwr1;
constexpr u32 VIEWKLUDGE = d::m3::viewkludge;
constexpr u32 BRDRCNT = d::m3::brdrcnt, SHAKECNT = d::m3::shakecnt, EJECTFLAG = d::m3::ejectflag;
constexpr u32 ROCKETFLAG = d::m3::rocketflag, FRMNUM = d::m3::frmnum, PGFLG = d::m3::pgflg, VBOFF = d::m3::vboff;
constexpr u32 JUMPFLAG = d::m3::jumpflag, JUMPVALUE = d::m3::jumpvalue, MAXOBJCOUNT = d::m3::maxobjcount;
// HLINE.ASM
constexpr u32 CLRUP = d::hline::clrup, CLRDN = d::hline::clrdn;
// F3DVEC.ASM
constexpr u32 OROT = d::f3dvec::orot, OMAT = d::f3dvec::omat, TMAT = d::f3dvec::tmat;
constexpr u32 OROTPTR = d::f3dvec::orotptr, NOROTPTR = d::f3dvec::norotptr;
constexpr u32 TEMPV = d::f3dvec::tempv, DRWVEC = d::f3dvec::drwvec, TEMPVCT = d::f3dvec::tempvct;
constexpr u32 TMPBUF = d::f3dvec::tmpbuf, TMP2BUF = d::f3dvec::tmp2buf, TESTPOLY = d::f3dvec::testpoly;
constexpr u32 PZMIN = d::f3dvec::pzmin, PZMAX = d::f3dvec::pzmax, PZMINBX = d::f3dvec::pzminbx;
constexpr u32 NSIDES = d::f3dvec::nsides, YSIGN = d::f3dvec::ysign;
constexpr u32 XC = d::f3dvec::xc, YC = d::f3dvec::yc, XW = d::f3dvec::xw;
constexpr u32 CIRTBL = d::f3dvec::cirtbl, COLORS = d::f3dvec::colors;
constexpr u32 GROUNDCOLORS = d::f3dvec::groundcolors, SKYCOLORS = d::f3dvec::skycolors;
constexpr u32 COLORBITS = d::f3dvec::colorbits, BRDRCLR = d::f3dvec::brdrclr;
constexpr u32 RESCOLORS = d::f3dvec::rescolors, HALUCLRS = d::f3dvec::haluclrs;
constexpr u32 Z0 = d::f3dvec::z0, Z0FACTOR = d::f3dvec::z0factor, Z0A13 = d::f3dvec::z0a13, Z0A33 = d::f3dvec::z0a33;
constexpr u32 TBLIST = d::f3dvec::tblist;
// MAT.ASM
constexpr u32 ROLVEC = d::mat::rolvec, ROLMAT = d::mat::rolmat, TMPMAT = d::mat::tmpmat;
// DEFOBJS.ASM
constexpr u32 EXTENDEDTYPETBL = d::defobjs::extendedtypetbl;

// 3DVID.INC rear-view window constants used directly by viewln/epage
constexpr int GTXMIN = 168, GTYMIN = 254, GTXMAX = 471, GTYMAX = 344;
constexpr int BPL = 80, BRTHK = 4, VYSIZ = 350;

inline i16 neg16(i16 v) { return static_cast<i16>(static_cast<u16>(0u - static_cast<u16>(v))); }
inline i16 add16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) + static_cast<u16>(b))); }
inline i16 sub16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) - static_cast<u16>(b))); }
inline i16 abs16(i16 v) { return v < 0 ? neg16(v) : v; }
inline u32 abs32(i32 v) { return v < 0 ? 0u - static_cast<u32>(v) : static_cast<u32>(v); }
inline i16 rolhi(i16 a, i16 b) { return asmx::rol1(asmx::hi16(imul16(a, b))); }
inline i16 proj(i16 coord, i16 pln, i16 depth) { return idiv32(imul16(neg16(coord), pln), depth).q; }

// sas.lib divrs_dw: signed dword / word, rounded (cf. the .divr_dw macro), dword result
i32 divrs_dw(i32 num, i16 den) {
    if (den == 0) return num >= 0 ? INT32_MAX : INT32_MIN;
    u32 n = abs32(num), dd = static_cast<u32>(den < 0 ? -static_cast<i32>(den) : den);
    u32 q = n / dd, r = n % dd;
    if (2 * r >= dd) q++;
    return ((num < 0) != (den < 0)) ? static_cast<i32>(0u - q) : static_cast<i32>(q);
}

// .muls_dw: low 32 bits of dword * signed word
inline u32 muls_dw(u32 v, i16 m) { return static_cast<u32>(static_cast<int64_t>(static_cast<i32>(v)) * m); }

} // namespace

F3dVec::F3dVec(GameState& gs, EgaScreen& scr) : m_g(gs), m_scr(scr), m_pal(EgaPalette::defaults()) {}

void F3dVec::movsw(u16 di, u16 si, int words) {
    for (int i = 0; i < words; i++, di += 2, si += 2) SW(di, W(si));
}

// sas.lib rand_w is not in the repo; a 16-bit LCG stands in until it is pinned down
u16 F3dVec::rand_w() {
    m_rand = static_cast<u16>(m_rand * 25173u + 13849u);
    return m_rand;
}

u8 F3dVec::colorOf(u16 word) const {
    // A color word is either an immediate (high byte 0) or the address of a color byte
    return (word >> 8) ? B(word) : static_cast<u8>(word);
}

// =====================================================================
// Matrices
// =====================================================================

void F3dVec::calcsin(u16 bx) {
    m_sya = ssin(W(bx));
    m_cya = scos(W(bx));
    m_spa = ssin(W(bx + 2));
    m_cpa = scos(W(bx + 2));
    m_sra = ssin(W(bx + 4));
    m_cra = scos(W(bx + 4));
}

void F3dVec::negsin() {
    m_sya = neg16(m_sya);
    m_spa = neg16(m_spa);
    m_sra = neg16(m_sra);
}

void F3dVec::calcmat(u16 si) {
    SW(si, hmul(m_cya, m_cpa));
    SW(si + 2, hmul(m_cpa, m_sya));
    SW(si + 4, neg16(m_spa));
    SW(si + 6, madd(hmul(hmul(m_sra, m_spa), m_cya), neg16(hmul(m_cra, m_sya))));
    SW(si + 8, madd(hmul(hmul(m_sra, m_spa), m_sya), hmul(m_cra, m_cya)));
    SW(si + 10, hmul(m_cpa, m_sra));
    SW(si + 12, madd(hmul(hmul(m_cra, m_spa), m_cya), hmul(m_sra, m_sya)));
    SW(si + 14, madd(hmul(hmul(m_cra, m_spa), m_sya), neg16(hmul(m_sra, m_cya))));
    SW(si + 16, hmul(m_cpa, m_cra));
}

void F3dVec::ncalcmat(u16 si) {
    negsin();
    SW(si, hmul(m_cya, m_cpa));
    i16 cx = hmul(m_cya, m_spa);
    SW(si + 2, madd(hmul(cx, m_sra), hmul(m_sya, m_cra)));            // a12
    SW(si + 4, madd(hmul(m_sya, m_sra), neg16(hmul(cx, m_cra))));     // a13
    SW(si + 6, neg16(hmul(m_sya, m_cpa)));                            // a21
    cx = hmul(m_sya, m_spa);
    SW(si + 8, madd(hmul(m_cya, m_cra), neg16(hmul(cx, m_sra))));     // a22
    SW(si + 10, madd(hmul(cx, m_cra), hmul(m_cya, m_sra)));           // a23
    SW(si + 12, m_spa);                                               // a31
    SW(si + 14, neg16(hmul(m_cpa, m_sra)));                           // a32
    SW(si + 16, hmul(m_cpa, m_cra));                                  // a33
}

void F3dVec::dot(u16 si, u16 bx, u16 di) {
    SW(di, hmul(W(si), W(bx)));
    i16 dx = hmul(W(si + 2), W(bx + 2));
    SW(di, madd(W(di), dx));
    dx = hmul(W(bx + 4), W(si + 4));
    SW(di, madd(W(di), dx));
}

void F3dVec::dot2(u16 si, u16 bx, u16 di) {
    u32 s = static_cast<u32>(imul16(W(si), W(bx)));
    s += static_cast<u32>(imul16(W(si + 6), W(bx + 2)));
    s += static_cast<u32>(imul16(W(si + 12), W(bx + 4)));
    i16 dx = static_cast<i16>(s >> 16);
    u16 ax = static_cast<u16>(s);
    if (!(dx < 16383)) dx = 16382;
    if (!(dx > -16383)) dx = -16382;
    SW(di, static_cast<i16>(static_cast<u16>((static_cast<u16>(dx) << 1) | (ax >> 15))));
}

void F3dVec::primdot(u16 si, u16 bx, u16 di) {
    SW(di, rolhi(W(si), W(bx)));
    SW(di, add16(W(di), rolhi(W(si + 2), W(bx + 2))));
    SW(di, add16(W(di), rolhi(W(si + 4), W(bx + 4))));
}

void F3dVec::matvmul(u16 si, u16 bx, u16 di) {
    dot(si, bx, di);
    dot(si, bx + 6, di + 2);
    dot(si, bx + 12, di + 4);
}

void F3dVec::mat2mul(u16 si, u16 bx, u16 di) {
    dot2(si, bx, di);
    dot2(si, bx + 6, di + 6);
    dot2(si, bx + 12, di + 12);
}

void F3dVec::pmvmul(u16 si, u16 bx, u16 di) {
    primdot(si, bx, di);
    primdot(si, bx + 6, di + 2);
    primdot(si, bx + 12, di + 4);
}

// F3DVEC matmul: omat[r][c] = orot row r . tmat column c
void F3dVec::matmul() {
    u16 orot = static_cast<u16>(W(OROTPTR));
    mat2mul(TMAT, orot, OMAT);
    mat2mul(TMAT + 2, orot, OMAT + 2);
    mat2mul(TMAT + 4, orot, OMAT + 4);
}

// MAT.ASM rolpln: rotates the matrix at [si] by roll/pitch/yaw (its own matmul:
// tmpmat = [si] * rolmat, then copied back)
void F3dVec::rolpln(i16 roll, i16 pitch, i16 yaw, u16 si) {
    SW(ROLVEC + 4, roll);
    SW(ROLVEC + 2, pitch);
    SW(ROLVEC, yaw);
    calcsin(ROLVEC);
    ncalcmat(ROLMAT);
    mat2mul(ROLMAT, si, TMPMAT);
    mat2mul(ROLMAT + 2, si, TMPMAT + 2);
    mat2mul(ROLMAT + 4, si, TMPMAT + 4);
    movsw(si, TMPMAT, 9);
}

// =====================================================================
// Windows and projection
// =====================================================================

Win F3dVec::win(u16 off) const {
    const int s = m_scr.scale();
    auto sc = [&](u32 o) { return static_cast<i16>(W(o) * s); };
    Win w{};
    w.xmin = sc(off + 0);
    w.ymin = sc(off + 2);
    w.xmax = sc(off + 4);
    w.ymax = sc(off + 6);
    w.xsiz = sc(off + 8);
    w.ysiz = sc(off + 10);
    w.zxpln = sc(off + 12);
    w.zypln = sc(off + 14);
    w.xctr = sc(off + 16);
    w.yctr = sc(off + 18);
    w.ymindi = W(off + 20);
    w.ymaxdi = W(off + 22);
    // rawhlinewin accepts rows with row*80 in [ymindi, ymaxdi)
    auto ceilRow = [](int di) { return di >= 0 ? (di + BPL - 1) / BPL : -(-di / BPL); };
    w.minRow = ceilRow(w.ymindi) * s;
    w.maxRow = ceilRow(w.ymaxdi) * s;
    w.fillRow = (static_cast<u16>(w.ymindi) / BPL) * s;
    w.fillX = (static_cast<u16>(w.ymindi) % BPL) * 8 * s;
    return w;
}

bool F3dVec::drawpt(const Win& w) {
    i16 x = W(ARGX1), y = W(ARGY1);
    if (sf16(x, w.xmin) || !sf16(x, w.xmax)) return true;
    if (sf16(y, w.ymin) || !sf16(y, w.ymax)) return true;
    m_scr.pset(x, y, B(N) & 15);
    return false;
}

bool F3dVec::f3dpt(u16 si, u16 bx) {
    Win w = win(bx);
    i16 di = W(si);
    if (di < 0) return true;
    if (!sf16(w.zxpln, di)) return true;
    SW(ARGX1, add16(proj(W(si + 2), w.zxpln, di), w.xctr));
    if (!sf16(w.zypln, di)) return true;
    SW(ARGY1, add16(proj(W(si + 4), w.zypln, di), w.yctr));
    return drawpt(w);
}

bool F3dVec::f3ddsc(u16 si, u16 bx) {
    Win w = win(bx);
    i16 di = W(si);
    if (di < 0) return true;
    if (!sf16(w.zxpln, di)) return true;
    SW(XC, add16(proj(W(si + 2), w.zxpln, di), w.xctr));
    SW(XW, idiv32(imul16(W(XW), w.zxpln), di).q);
    if (!sf16(w.zypln, di)) return true;
    SW(YC, add16(proj(W(si + 4), w.zypln, di), w.yctr));
    return drawdisc(w);
}

// Moves the nearer endpoint of the line at [si] onto the plane z = zxpln+1.
// Shared by choplin and the front of f3dline.
bool F3dVec::choplin(u16 si, u16 bx) {
    const i16 zp = win(bx).zxpln;
    i16 a = sub16(W(si), zp), c = sub16(W(si + 6), zp);
    if ((a & c) & 0x8000) return true;      // both behind
    if (!((a | c) & 0x8000)) return false;  // both in front
    if (!sf16(W(si), W(si + 6))) {          // order so z0 < z1
        movsw(TEMPV, si, 3);
        movsw(si, si + 6, 3);
        movsw(si + 6, TEMPV, 3);
    }
    i16 z0 = W(si), z1 = W(si + 6);
    if (!sf16(zp, z1)) return true;
    i16 dz = sub16(zp, z0);
    if (dz < 0) return true;
    i16 span = sub16(z1, z0);
    SW(si + 2, add16(W(si + 2), idiv32(imul16(sub16(W(si + 8), W(si + 2)), dz), span).q));
    SW(si, add16(zp, 1));
    SW(si + 4, add16(W(si + 4), idiv32(imul16(sub16(W(si + 10), W(si + 4)), dz), span).q));
    return false;
}

void F3dVec::scode1(const Win& w) {
    m_code1 = 0;
    i16 x = W(ARGX1), y = W(ARGY1);
    if (sf16(x, w.xmin)) m_code1 |= 1;
    if (!(sf16(x, w.xmax) || x == w.xmax)) m_code1 |= 4;
    if (sf16(y, w.ymin)) m_code1 |= 2;
    if (!(sf16(y, w.ymax) || y == w.ymax)) m_code1 |= 8;
}

void F3dVec::scode2(const Win& w) {
    m_code2 = 0;
    i16 x = W(ARGX2), y = W(ARGY2);
    if (sf16(x, w.xmin)) m_code2 |= 1;
    if (!(sf16(x, w.xmax) || x == w.xmax)) m_code2 |= 4;
    if (sf16(y, w.ymin)) m_code2 |= 2;
    if (!(sf16(y, w.ymax) || y == w.ymax)) m_code2 |= 8;
}

void F3dVec::csetup() {
    m_delx = sub16(W(ARGX2), W(ARGX1));
    m_dely = sub16(W(ARGY2), W(ARGY1));
}

i16 F3dVec::yline1(i16 ax) {
    return add16(idiv32(imul16(sub16(ax, W(ARGX1)), m_dely), m_delx).q, W(ARGY1));
}

i16 F3dVec::xline(i16 ax) {
    return add16(idiv32(imul16(sub16(ax, W(ARGY1)), m_delx), m_dely).q, W(ARGX1));
}

void F3dVec::fixx1a(const Win& w) {
    csetup();
    if (m_code1 & 1) { SW(ARGY1, yline1(w.xmin)); SW(ARGX1, w.xmin); }
    if (m_code1 & 4) { SW(ARGY1, yline1(w.xmax)); SW(ARGX1, w.xmax); }
}

void F3dVec::fixx1b(const Win& w) {
    csetup();
    if (m_code1 & 2) { SW(ARGX1, xline(w.ymin)); SW(ARGY1, w.ymin); }
    if (m_code1 & 8) { SW(ARGX1, xline(w.ymax)); SW(ARGY1, w.ymax); }
}

void F3dVec::fixx2a(const Win& w) {
    csetup();
    if (m_code2 & 1) { SW(ARGY2, yline1(w.xmin)); SW(ARGX2, w.xmin); }
    if (m_code2 & 4) { SW(ARGY2, yline1(w.xmax)); SW(ARGX2, w.xmax); }
}

void F3dVec::fixx2b(const Win& w) {
    csetup();
    if (m_code2 & 2) { SW(ARGX2, xline(w.ymin)); SW(ARGY2, w.ymin); }
    if (m_code2 & 8) { SW(ARGX2, xline(w.ymax)); SW(ARGY2, w.ymax); }
}

// Clips x first, re-tests, then y without a second test. Carry = not drawn.
bool F3dVec::oclip(const Win& w) {
    fixx1a(w);
    fixx2a(w);
    scode1(w);
    scode2(w);
    if ((m_code1 | m_code2) == 0) return false;
    if (m_code1 & m_code2) return true;
    fixx1b(w);
    fixx2b(w);
    return false;
}

bool F3dVec::f3dline(u16 si, u16 bx) {
    m_lineptr = si;
    if (choplin(si, bx)) return true;
    Win w = win(bx);
    auto project = [&](u16 v, u32 ox, u32 oy) {
        i16 di = W(v);
        if (di <= 0) return false;
        if (m_rdf == 0 && !sf16(w.zxpln, di)) return false;
        SW(ox, add16(proj(W(v + 2), w.zxpln, di), w.xctr));
        if (m_rdf == 0 && !sf16(w.zypln, di)) return false;
        SW(oy, add16(proj(W(v + 4), w.zypln, di), w.yctr));
        return true;
    };
    if (!project(si, ARGX1, ARGY1)) return true;
    if (!project(si + 6, ARGX2, ARGY2)) return true;
    scode1(w);
    scode2(w);
    if (m_code1 | m_code2) {
        if (m_code1 & m_code2) return true;
        if (oclip(w)) return true;
    }
    if (B(HRFLG) & B(FILLFLG)) return hline(w);
    kline();
    return false;
}

bool F3dVec::polyhell() {
    const u16 nsides = static_cast<u16>(W(NSIDES));
    m_nside6 = static_cast<u16>(6 * nsides);
    // extend the ring so it appears to start at the nearest point
    const u16 count = static_cast<u16>(W(PZMINBX) + 6);
    for (u16 i = 0; i < count; i++) SB(TMPBUF + m_nside6 + i, B(TMPBUF + i));

    u16 bx = static_cast<u16>(W(PZMINBX));
    m_pzcount = static_cast<i16>(nsides + 1);
    for (;;) {
        bx += 6;
        if (--m_pzcount == 0) return true;
        if (!(W(TMPBUF + bx) < m_pzypln)) break;
    }
    movsw(TMP2BUF, TMPBUF + bx - 6, 6);
    m_lineptr = TMP2BUF;
    if (choplin(TMP2BUF, m_polywin)) return true;
    m_lindex = 12;
    for (;;) {
        bx += 6;
        if (--m_pzcount == 0) return true;
        movsw(TMP2BUF + m_lindex, TMPBUF + bx, 6);
        if (W(TMPBUF + bx) < m_pzypln) break;
        m_lindex += 6;
    }
    m_lindex -= 6;
    const u16 si = TMP2BUF + m_lindex;
    m_lineptr = si;
    if (choplin(si, m_polywin)) {
        SW(TMP2BUF + 2, static_cast<i16>(m_lindex));
        return true;
    }
    // the chopped line comes back near-first; reverse it
    movsw(TEMPV, si, 3);
    movsw(si, si + 6, 3);
    movsw(si + 6, TEMPV, 3);
    SW(NSIDES, static_cast<i16>(static_cast<u8>(m_lindex / 6) + 2));
    m_lindex = 0;
    return false;
}

bool F3dVec::f3dpoly(u16 si, u16 bx) {
    m_polywin = bx;
    Win w = win(bx);
    m_pxmin = m_pymin = 32767;
    m_pxmax = m_pymax = -32767;
    m_lindex = 0;
    m_pzypln = w.zxpln;
    const i16 zmax = W(PZMAX);
    if (zmax < 0) return true;
    if (w.zxpln >= zmax) return true;   // whole polygon too close
    if (!(w.zxpln <= W(PZMIN))) {       // partly under us: clip it
        if (polyhell()) return true;
        si = TMP2BUF;
    }
    u16 cx = static_cast<u16>(W(NSIDES));
    do {
        SW(PZMINBX, -1);
        i16 di = W(si);
        SW(PZMINBX, -2);
        if (di < 0) return true;
        if (m_rdf == 0) {
            SW(PZMINBX, -3);
            if (!sf16(w.zxpln, di)) return true;
        }
        i16 x = add16(proj(W(si + 2), w.zxpln, di), w.xctr);
        SW(ARGX1, x);
        if (m_rdf == 0) {
            SW(PZMINBX, -4);
            if (!sf16(w.zypln, di)) return true;
        }
        i16 y = add16(proj(W(si + 4), w.zypln, di), w.yctr);
        if (!(y >= m_pymin)) m_pymin = y;
        if (!(y <= m_pymax)) m_pymax = y;
        if (!(x >= m_pxmin)) m_pxmin = x;
        if (!(x <= m_pxmax)) m_pxmax = x;
        SW(TESTPOLY + m_lindex, x);
        SW(TESTPOLY + m_lindex + 2, y);
        m_lindex += 4;
        si += 6;
    } while (--cx);

    SW(PZMINBX, -6);
    if (m_pxmax <= w.xmin) return true;
    SW(PZMINBX, -7);
    if (m_pxmin >= w.xmax) return true;
    SW(PZMINBX, -8);
    if (m_pymax <= w.ymin) return true;
    SW(PZMINBX, -9);
    if (m_pymin >= w.ymax) return true;
    drawpoly(w, TESTPOLY);
    return false;
}

// =====================================================================
// Scan conversion
// =====================================================================

// Pixels [argx1, argx2) of a row, clipped to the window, in m_color through the
// dither mask. The ASM's first-byte mask shift in the >16 pixel case is not
// reproduced; the pattern is applied by absolute pixel position throughout.
void F3dVec::rawhlinewin(const Win& w, int row) {
    if (row < w.minRow || row >= w.maxRow) return;
    i16 ax = W(ARGX2), bx = W(ARGX1);
    if (ax < bx) std::swap(ax, bx);
    if (ax < w.xmin) return;
    if (!(ax < w.xmax)) ax = w.xmax;
    if (bx >= w.xmax) return;
    if (bx < w.xmin) bx = w.xmin;
    if (ax == bx) return;
    if (m_hlinemask == 0xFF) {
        m_scr.hspan(bx, ax, row, m_color);
        return;
    }
    for (int x = bx; x < ax; x++)
        if ((m_hlinemask >> (7 - (x & 7))) & 1) m_scr.pset(x, row, m_color);
}

bool F3dVec::ladvance(u16 si) {
    int levelcnt = 1;
    for (;;) {
        m_xl1 = m_xl2;
        SW(ARGX1, m_xl2);
        m_yl1 = m_yl2;
        u16 bx = static_cast<u16>(m_lindex + 4);   // pupring
        if (!(bx < m_nside4)) bx = 0;
        m_lindex = bx;
        m_xl2 = W(si + bx);
        m_yl2 = W(si + bx + 2);
        i16 dy = sub16(m_yl2, m_yl1);
        if (dy < 0) return true;
        if (dy == 0) {
            if (--levelcnt >= 0) continue;
            return true;
        }
        i16 dx = sub16(m_xl2, m_xl1);
        // "cwd / xchg ax,dx": high word = dx, low word = its sign extension
        u32 num = (static_cast<u32>(static_cast<u16>(dx)) << 16) | (dx < 0 ? 0xFFFFu : 0u);
        m_ldeltax = divrs_dw(static_cast<i32>(num), dy);
        m_argx1low = 0x7fff;
        return false;
    }
}

bool F3dVec::radvance(u16 si) {
    int levelcnt = 1;
    for (;;) {
        m_xr1 = m_xr2;
        SW(ARGX2, m_xr2);
        m_yr1 = m_yr2;
        i16 bx = static_cast<i16>(m_rindex - 4);   // pdnring
        if (bx < 0) bx = static_cast<i16>(m_nside4 - 4);
        m_rindex = static_cast<u16>(bx);
        m_xr2 = W(si + m_rindex);
        m_yr2 = W(si + m_rindex + 2);
        i16 dy = sub16(m_yr2, m_yr1);
        if (dy < 0) return true;
        if (dy == 0) {
            if (--levelcnt >= 0) continue;
            return true;
        }
        i16 dx = sub16(m_xr2, m_xr1);
        u32 num = (static_cast<u32>(static_cast<u16>(dx)) << 16) | (dx < 0 ? 0xFFFFu : 0u);
        m_rdeltax = divrs_dw(static_cast<i32>(num), dy);
        m_argx2low = 0x7fff;
        return false;
    }
}

void F3dVec::drawpoly(const Win& w, u16 bx) {
    m_polyymax = w.ymax;
    m_polyymin = w.ymin;
    const u16 si = bx;
    SW(ARGY1, -32768);
    int row = 0;

    auto getX1 = [&]() { return (static_cast<u32>(static_cast<u16>(W(ARGX1))) << 16) | m_argx1low; };
    auto getX2 = [&]() { return (static_cast<u32>(static_cast<u16>(W(ARGX2))) << 16) | m_argx2low; };
    auto setX1 = [&](u32 v) { SW(ARGX1, static_cast<i16>(v >> 16)); m_argx1low = static_cast<u16>(v); };
    auto setX2 = [&](u32 v) { SW(ARGX2, static_cast<i16>(v >> 16)); m_argx2low = static_cast<u16>(v); };

    const i16 nsides = W(NSIDES);
    if (nsides <= 0 || static_cast<u16>(nsides) > 20) goto polydone;
    {
        m_nside4 = static_cast<u16>(nsides * 4);
        i16 ymin = 32767, ymax = -32767;
        for (u16 i = 0, b = 0; i < static_cast<u16>(nsides); i++, b += 4) {
            i16 y = W(si + b + 2);
            if (!(y > ymin)) { ymin = y; m_lindex = b; }
            if (!(y < ymax)) ymax = y;
        }
        m_pymax = ymax;
        SW(ARGY1, ymin);
        row = ymin;
        m_rindex = m_lindex;
    }
    m_xl2 = W(si + m_lindex);
    SW(ARGX1, m_xl2);
    SW(ARGX2, m_xl2);
    m_yl2 = W(si + m_lindex + 2);
    if (ladvance(si)) goto polydone;
    m_xr2 = W(si + m_rindex);
    m_yr2 = W(si + m_rindex + 2);
    if (radvance(si)) goto polydone;

    m_color = B(N);
    if (m_color > 15) {
        // The set/reset register ends up loaded with the dither mask, not the
        // color (the "and ah,15" result is overwritten); kept as in the original
        u8 m = static_cast<u8>(rand_w() & 0x55);
        m_hlinemask = m;
        m_color = m & 15;
    }

    for (;;) {
        if (!(m_yl2 > W(ARGY1))) {
            if (ladvance(si)) goto polydone;
            if (m_pymax <= m_yl1) goto polydone;
        }
        if (!(m_yr2 > W(ARGY1))) {
            if (radvance(si)) goto polydone;
        }
        if (W(ARGY1) < m_polyymin) goto skipahead;
        if (!(W(ARGY1) < m_polyymax)) goto polydone;
    noabovedone:
        if (!(W(ARGY1) < m_polyymin)) {
            if (m_hlinemask != 0xFF) m_hlinemask = static_cast<u8>(rand_w() & 0x55);
            rawhlinewin(w, row);
        }
        row++;
        SW(ARGY1, add16(W(ARGY1), 1));
        setX1(getX1() + static_cast<u32>(m_ldeltax));
        setX2(getX2() + static_cast<u32>(m_rdeltax));
        continue;

    skipahead:
        // jump straight to the top of the window instead of stepping there
        if (m_yl2 <= m_yr2) goto l2smaller;
        if (m_yr2 > m_polyymin) goto advboth;
        if (radvance(si)) goto polydone;
        if (m_pymax <= m_yr1) goto polydone;
        SW(ARGX2, m_xr1);
        {
            i16 to = m_yr1, from = W(ARGY1);
            SW(ARGY1, to);
            i16 n = sub16(to, from);
            if (n < 0) goto polydone;
            setX1(getX1() + muls_dw(static_cast<u32>(m_ldeltax), n));
        }
        goto skipadone;
    l2smaller:
        if (m_yl2 > m_polyymin) goto advboth;
        if (ladvance(si)) goto polydone2;
        if (m_pymax <= m_yl1) goto polydone2;
        SW(ARGX1, m_xl1);
        {
            i16 to = m_yl1, from = W(ARGY1);
            SW(ARGY1, to);
            i16 n = sub16(to, from);
            if (n < 0) goto polydone2;
            setX2(getX2() + muls_dw(static_cast<u32>(m_rdeltax), n));
        }
        goto skipadone;
    advboth: {
        i16 n = sub16(m_polyymin, W(ARGY1));
        if (n >= 0) {
            SW(ARGY1, m_polyymin);
            setX1(getX1() + muls_dw(static_cast<u32>(m_ldeltax), n));
            setX2(getX2() + muls_dw(static_cast<u32>(m_rdeltax), n));
        }
    }
    skipadone:
        row = W(ARGY1);
        goto noabovedone;
    }

polydone:
    m_hlinemask = 0xFF;
polydone2:
    return;
}

// HLINE.ASM: draws the horizon from (argx1,argy1)-(argx2,argy2) and fills the
// window on either side with the sky and ground colors. The byte-oriented EGA
// writes are done per pixel; the per-row split points follow the same DDA.
bool F3dVec::hline(const Win& w) {
    const u8 up = B(CLRUP), dn = B(CLRDN);
    u8 clrbot = up, clrtop = dn, clrlft = dn, clrrgt = up;
    i16 x1 = W(ARGX1), y1 = W(ARGY1), x2 = W(ARGX2), y2 = W(ARGY2);
    i16 ylmax = y2, ylmin = y1;
    i16 dy = sub16(y2, y1);
    if (dy < 0) {
        clrrgt = dn;
        clrlft = up;
        std::swap(ylmin, ylmax);
    }
    i16 distsum = abs16(dy);
    i16 minx = x1;
    i16 dx = sub16(x2, x1);
    if (dx < 0) {
        minx = x2;
        clrbot = dn;
        clrtop = up;
    }
    if (!(add16(abs16(dx), distsum) > 8)) return true;   // too short to give a slope

    const u16 wxbytes = static_cast<u16>((static_cast<u16>(w.xsiz - 1) >> 3) + 1);
    const u16 nleft = static_cast<u16>(static_cast<u16>(minx - w.xmin) >> 3);
    const int fillX = (static_cast<u16>(w.xmin) >> 3) * 8;
    const int spanL = ((static_cast<u16>(minx) >> 3) - nleft) * 8;
    const int spanR = spanL + wxbytes * 8;

    // solid rows above and below the line's extent
    i16 cx = sub16(ylmin, w.ymin);
    if (cx > 0)
        for (int r = 0; r <= cx; r++) m_scr.hspan(fillX, fillX + wxbytes * 8, w.ymin + r, clrbot);
    cx = sub16(w.ymax, ylmax);
    if (cx > 0)
        for (int r = 0; r < cx; r++) m_scr.hspan(fillX, fillX + wxbytes * 8, ylmax + r, clrtop);

    // walk the line left to right, one split point per row
    i16 ax = sub16(x2, x1);
    if (ax < 0) {
        ax = neg16(ax);
        std::swap(x1, x2);
        std::swap(y1, y2);
    }
    int lincr = 1;
    i16 rows = sub16(y2, y1);
    if (rows < 0) {
        lincr = -1;
        rows = neg16(rows);
    }
    rows = add16(rows, 1);

    auto split = [&](int row, int x) {
        int sx = std::clamp(x, spanL, spanR);
        m_scr.hspan(spanL, sx, row, clrlft);
        m_scr.hspan(sx, spanR, row, clrrgt);
    };
    // big steps when the 1.15 accumulator is non-negative, as in bbyte/sbyte
    auto ddaSetup = [](i16 num, i16 den, i16& whole, i16& smladd, i16& bigadd) {
        whole = static_cast<i16>(num / den);
        i16 rem = static_cast<i16>(num % den);
        smladd = idiv32(static_cast<i32>(static_cast<u32>(static_cast<u16>(rem >> 1)) << 16), den).q;
        bigadd = sub16(smladd, static_cast<i16>(0x8000));
    };

    int x = x1, row = y1;
    i16 acc = 0;
    bool big = true;
    if (sf16(rows, ax) || rows == ax) {
        // shallow: one row per step, npxps or npxps+1 pixels along
        if (ax == 0) return true;
        i16 npxps, smladd, bigadd;
        ddaSetup(ax, rows, npxps, smladd, bigadd);
        for (i16 n = rows; n > 0; n--) {
            split(row, x);
            x += big ? npxps + 1 : npxps;
            row += lincr;
            if (n == 1) break;
            acc = add16(acc, big ? bigadd : smladd);
            big = acc >= 0;
        }
    } else {
        // steep: one pixel along per step, wnsml or wnbig rows each
        if (ax == 0) ax = 1;
        i16 npxps, smladd, bigadd;
        ddaSetup(rows, ax, npxps, smladd, bigadd);
        for (i16 n = ax; n > 0; n--) {
            int k = big ? npxps + 1 : npxps;
            for (; k > 0; k--) {
                split(row, x);
                row += lincr;
            }
            x++;
            if (n == 1) break;
            acc = add16(acc, big ? bigadd : smladd);
            big = acc >= 0;
        }
    }
    return false;
}

// KLINE.ASM: horizontal lines exclude the right end (horline); everything else
// goes through EGALINE's Bresenham, both ends inclusive. KLINE's run-slice path
// for long shallow lines plots the same span per row, give or take a pixel.
void F3dVec::kline() {
    const u8 c = B(N) & 15;
    int x1 = W(ARGX1), y1 = W(ARGY1), x2 = W(ARGX2), y2 = W(ARGY2);
    if (x2 < x1) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }
    if (y1 == y2) {
        for (int x = x1; x < x2; x++) m_scr.pset(x, y1, c);
        return;
    }
    int dx = x2 - x1, dy = std::abs(y2 - y1), sy = y2 > y1 ? 1 : -1;
    if (dx == 0) {
        for (int y = std::min(y1, y2); y <= std::max(y1, y2); y++) m_scr.pset(x1, y, c);
        return;
    }
    if (dy <= dx) {
        int d = 2 * dy - dx;
        for (int x = x1, y = y1; x <= x2; x++) {
            m_scr.pset(x, y, c);
            if (d >= 0) { y += sy; d += 2 * (dy - dx); }
            else d += 2 * dy;
        }
    } else {
        int d = 2 * dx - dy;
        for (int y = y1, x = x1, n = 0; n <= dy; n++, y += sy) {
            m_scr.pset(x, y, c);
            if (d >= 0) { x++; d += 2 * (dx - dy); }
            else d += 2 * dx;
        }
    }
}

bool F3dVec::drawdisc(const Win& w) {
    const int s = m_scr.scale();
    const i16 xc = W(XC), yc = W(YC);
    i16 ax = W(XW);
    if (!(ax < 1024)) ax = 1023;
    const u16 yw = static_cast<u16>((static_cast<u32>(static_cast<u16>(ax)) * 45397u) >> 16);
    const i16 half = static_cast<i16>(static_cast<i16>(yw) >> 1);
    i16 t = sub16(yc, half);
    if (t > w.ymax) return static_cast<u16>(t) < static_cast<u16>(w.ymax);
    t = add16(yc, half);
    if (t < w.ymin) return static_cast<u16>(t) < static_cast<u16>(w.ymin);
    if (ax < 0) return false;
    m_color = B(N) & 15;

    if (sf16(ax, 17)) {
        // smldisc: bitmap from cirtbl, bit 15 of each row word at x-8
        if (sf16(xc, w.xmin)) return static_cast<u16>(xc) < static_cast<u16>(w.xmin);
        if (!sf16(xc, w.xmax)) return static_cast<u16>(xc) < static_cast<u16>(w.xmax);
        if (sf16(yc, w.ymin)) return static_cast<u16>(yc) < static_cast<u16>(w.ymin);
        if (!sf16(yc, w.ymax)) return static_cast<u16>(yc) < static_cast<u16>(w.ymax);
        u16 tbl = static_cast<u16>(W(CIRTBL + 2 * static_cast<u16>(W(XW))));
        u16 rows = static_cast<u16>(W(tbl));
        for (u16 p = tbl + 2; rows; rows--, p += 4) {
            int dr = W(p) / BPL;
            u16 m = static_cast<u16>(W(p + 2));
            for (int k = 0; k < 16; k++)
                if (m & (0x8000 >> k)) m_scr.pset(xc - 8 + k, yc + dr, m_color);
        }
        return false;
    }

    // big disc: scanlines from the center out using the csqrt profile
    if (abs32(imul16(static_cast<i16>(yc / s), BPL)) >> 16) return false;
    const u16 xw = static_cast<u16>(W(XW));
    const u32 ysf = (2936u << 16) / xw;                    // div_dw
    const u16 ysfig = static_cast<u16>((ysf << 4) >> 16);
    SW(ARGX1, sub16(xc, static_cast<i16>(xw >> 1)));
    SW(ARGX2, add16(add16(static_cast<i16>(static_cast<u16>(xw + 1) >> 1), xc), 1));
    int yline = yc, tyline = yc;
    rawhlinewin(w, yline);
    u32 acc = 0;
    for (u16 n = static_cast<u16>(static_cast<u16>(yw + 1) >> 1); n; n--) {
        acc += ysf;
        if (yline + 1 < w.maxRow) yline++;
        if (ysfig == 0) break;
        u16 q = static_cast<u16>(static_cast<u16>(csqrt(static_cast<i16>(acc >> 16)) << 5) / ysfig);
        SW(ARGX2, add16(add16(static_cast<i16>(static_cast<u16>(q + 1) >> 1), xc), 1));
        SW(ARGX1, sub16(xc, static_cast<i16>(q >> 1)));
        rawhlinewin(w, yline);
        if (tyline - 1 >= 0) tyline--;
        rawhlinewin(w, tyline);
    }
    return false;
}

// =====================================================================
// Views: forward window plus the rear window (ptrgwint) if it is on
// =====================================================================

void F3dVec::scrfbx(int row, int x, int rows, int widthPx, u8 color) {
    m_scr.fillRect(x, row, widthPx, rows, color);
}

void F3dVec::xvect(u16 si, u16 bx) {
    i16 z = W(bx);
    if (B(HRFLG) == 0) {
        SW(si, neg16(z));
        SW(si + 2, neg16(W(bx + 2)));
        SW(si + 4, W(bx + 4));
    } else {   // the horizon flips only vertically
        SW(si, z);
        SW(si + 2, W(bx + 2));
        SW(si + 4, neg16(W(bx + 4)));
    }
}

void F3dVec::viewln(u16 si) {
    const int s = m_scr.scale();
    movsw(TEMPVCT, si, 6);
    m_linefail = 0;
    const u16 f = static_cast<u16>(W(PTRGWINF));
    if (f3dline(si, f)) {
        m_linefail = 2;
        if (B(HRFLG)) {
            // the horizon is off screen: the whole window is sky or ground
            u8 c = static_cast<int8_t>(m_dvflg) < 0 ? B(CLRDN) : B(CLRUP);
            Win w = win(f);
            int words = static_cast<u16>(W(f + 8) + 1) >> 4;
            scrfbx(w.fillRow, w.fillX, W(f + 10) * s, words * 16 * s, c);
        }
    }
    const u16 t = static_cast<u16>(W(PTRGWINT));
    if (t) {
        m_linefail >>= 1;
        xvect(si, TEMPVCT);
        xvect(si + 6, TEMPVCT + 6);
        if (f3dline(si, t)) {
            m_linefail++;
            if (B(HRFLG)) {
                u8 c = static_cast<int8_t>(m_dvflg) < 0 ? B(CLRUP) : B(CLRDN);
                scrfbx(GTYMIN * s, GTXMIN * s, (GTYMAX - GTYMIN) * s, ((GTXMAX - GTXMIN + 1) / 16) * 16 * s, c);
            }
        }
    }
}

void F3dVec::viewpoly(u16 si) {
    f3dpoly(si, static_cast<u16>(W(PTRGWINF)));
    const u16 t = static_cast<u16>(W(PTRGWINT));
    if (!t) return;
    SW(PZMIN, 32767);
    SW(PZMAX, -32767);
    m_polyptr = 0;
    u16 v = si;
    u16 cx = static_cast<u16>(W(NSIDES));
    do {
        xvect(v, v);
        i16 z = W(v);
        if (!(z <= W(PZMAX))) SW(PZMAX, z);
        if (!(z >= W(PZMIN))) {
            SW(PZMIN, z);
            SW(PZMINBX, static_cast<i16>(m_polyptr));
        }
        m_polyptr += 6;
        v += 6;
    } while (--cx);
    f3dpoly(si, t);
}

void F3dVec::viewpt(u16 si) {
    if (f3dpt(si, static_cast<u16>(W(PTRGWINF)))) m_objfail++;
    const u16 t = static_cast<u16>(W(PTRGWINT));
    if (!t) return;
    xvect(si, si);
    if (f3dpt(si, t)) m_objfail++;
}

void F3dVec::viewdsc(u16 si) {
    if (f3ddsc(si, static_cast<u16>(W(PTRGWINF)))) m_objfail++;
    const u16 t = static_cast<u16>(W(PTRGWINT));
    if (!t) return;
    xvect(si, si);
    if (f3ddsc(si, t)) m_objfail++;
}

// =====================================================================
// Objects
// =====================================================================

// dst = origin (orgptr) + M * vertex, the unrolled loop in drwire/drpoly/drawrcld
void F3dVec::projectVertex(u16 si, u16 bx, u16 dst) {
    i16 v0 = W(si), v1 = W(si + 2), v2 = W(si + 4);
    if (m_rdf) {
        v0 = static_cast<i16>(v0 >> m_rdf);
        v1 = static_cast<i16>(v1 >> m_rdf);
        v2 = static_cast<i16>(v2 >> m_rdf);
    }
    movsw(dst, m_orgptr, 3);
    for (int r = 0; r < 3; r++) {
        u16 m = static_cast<u16>(bx + 6 * r), o = static_cast<u16>(dst + 2 * r);
        SW(o, add16(W(o), rolhi(W(m), v0)));
        SW(o, add16(W(o), rolhi(W(m + 2), v1)));
        SW(o, add16(W(o), rolhi(W(m + 4), v2)));
    }
}

void F3dVec::drwire(u16 si, u16 bx) {
    u16 cx = static_cast<u16>(W(si));
    SB(N, colorOf(static_cast<u16>(W(si + 2))));
    si += 4;
    if (!cx) return;   // the ASM would loop 65536 times on an empty wire
    do {
        projectVertex(si, bx, DRWVEC);
        projectVertex(si + 6, bx, DRWVEC + 6);
        viewln(DRWVEC);
        if (m_linefail > 1) m_objfail++;
        si += 6;
    } while (--cx);
}

void F3dVec::drpoly(u16 si, u16 bx) {
    u16 cx = static_cast<u16>(W(si));
    SW(NSIDES, static_cast<i16>(cx));
    SB(N, colorOf(static_cast<u16>(W(si + 2))));
    si += 4;
    m_polyptr = 0;
    SW(PZMIN, 32767);
    SW(PZMAX, -32767);
    if (!cx) return;
    do {
        const u16 dst = TMPBUF + m_polyptr;
        projectVertex(si, bx, dst);
        i16 z = W(dst);
        if (!(z <= W(PZMAX))) SW(PZMAX, z);
        if (!(z >= W(PZMIN))) {
            SW(PZMIN, z);
            SW(PZMINBX, static_cast<i16>(m_polyptr));
        }
        m_polyptr += 6;
        si += 6;
    } while (--cx);
    viewpoly(TMPBUF);
}

void F3dVec::drawrcld(u16 si, u16 bx) {
    u16 cx = static_cast<u16>(W(si));
    si += 2;
    if (!cx) return;
    do {
        SB(N, colorOf(static_cast<u16>(W(si + 6))));
        projectVertex(si, bx, DRWVEC);
        viewpt(DRWVEC);
        si += 8;
    } while (--cx);
}

void F3dVec::drawrdsc(u16 si, u16 bx) {
    u16 cx = static_cast<u16>(W(si));
    si += 2;
    if (!cx) return;
    do {
        SW(XW, static_cast<i16>(static_cast<u16>(W(si + 6)) >> m_rdf));
        SB(N, colorOf(static_cast<u16>(W(si + 8))));
        projectVertex(si, bx, DRWVEC);
        viewdsc(DRWVEC);
        si += 10;
    } while (--cx);
}

void F3dVec::drrfrm(u16 si, u16 bx) {
    u16 cx = static_cast<u16>(W(si));
    si += 2;
    for (; cx; cx--, si += 2) drwire(static_cast<u16>(W(si)), bx);
    m_objfail = 0;   // a frame never fails
}

// Draws the shape at [si] with matrix [bx]. The component list is picked by the
// octant the viewer sees the object from (painter's order baked into DEFOBJS).
// Carry (true) = drawing aborted because a far object failed to project.
bool F3dVec::drrobj(u16 si, u16 bx, u16 /*dx window: the views use ptrgwinf*/) {
    u16 ysign = 0;
    for (int c = 0; c < 3; c++) {
        i16 cx = hmul(W(XVEC), W(TMAT + 2 * c));
        cx = add16(cx, hmul(W(XVEC + 2), W(TMAT + 6 + 2 * c)));
        cx = add16(cx, hmul(W(XVEC + 4), W(TMAT + 12 + 2 * c)));
        ysign = static_cast<u16>((ysign << 1) | (static_cast<u16>(cx) >> 15));
    }
    SW(YSIGN, static_cast<i16>(ysign));
    m_orgptr = si;
    const u16 n = static_cast<u16>(W(si + 6));
    si = static_cast<u16>(si + static_cast<u16>((n & 0xFF) * (ysign & 0xFF)) * 4 + 8);
    for (u16 k = n; k; k--, si += 4) {
        m_objfail = 0;
        i16 code = W(si);
        u16 addr = static_cast<u16>(W(si + 2));
        if (code < 0) cecode |= 5;
        else if (!sf16(code, 9) || (code & 1)) cecode |= 6;
        else switch (code) {
            case 2: drrfrm(addr, bx); break;
            case 4: drawrcld(addr, bx); break;
            case 6: drawrdsc(addr, bx); break;
            case 8: drpoly(addr, bx); break;
            default: break;   // 0: nothing
        }
        if (m_rdf) {
            if (W(PTRGWINT) == 0 && m_objfail) return true;
            if (m_objfail == 2) return true;
        }
    }
    return false;
}

void F3dVec::relrot(u16 si) {
    calcsin(si + 12);
    ncalcmat(TMAT);
    matvmul(XVEC, static_cast<u16>(W(OROTPTR)), m_objptr);
    matmul();
}

void F3dVec::nrelrot(u16 /*si*/) {
    movsw(OMAT, OROT, 9);
    matvmul(XVEC, static_cast<u16>(W(OROTPTR)), m_objptr);
}

void F3dVec::gnrlrot(u16 /*si*/) {
    pmvmul(XVEC, static_cast<u16>(W(OROTPTR)), m_objptr);
}

void F3dVec::drawdirect(u16 si, u16 dx) {
    m_objptr = si;
    nrelrot(si);
    drrobj(si, OMAT, dx);
}

void F3dVec::drawrdirect(u16 si, u16 dx) {
    m_rdf = 0;
    m_objptr = static_cast<u16>(W(si + 40));
    relrot(si);
    drrobj(m_objptr, OMAT, dx);
}

void F3dVec::reducecoords() {
    const i16 f = W(Z0FACTOR);
    m_range = 0;
    for (int k = 0; k < 3; k++) {
        const u32 o = DXVEC + 4 * k;
        int64_t p = static_cast<int64_t>(m_g.dw(o)) * f;   // muls_qw
        i16 hi = static_cast<i16>(static_cast<uint64_t>(p) >> 32);
        m_g.setDw(o, static_cast<i32>(static_cast<uint64_t>(p) >> 15));
        m_range |= static_cast<u16>(abs16(hi));
    }
}

bool F3dVec::rel3d(u16 si) {
    m_range = 0;
    for (int k = 0; k < 3; k++) {
        i32 dd = static_cast<i32>(static_cast<u32>(m_g.dw(si + 4 * k)) - static_cast<u32>(m_g.dw(OX + 4 * k)));
        m_g.setDw(DXVEC + 4 * k, dd);
        m_range |= static_cast<u16>(abs32(dd) >> 15);
    }
    u16 z0 = static_cast<u16>(W(Z0));
    if (z0) {
        // map mode: pull things below the viewer in so they fit
        i16 below = neg16(W(DXVEC + 10));
        if (below >= 0 && static_cast<u16>(below) > z0) {
            u32 dz = abs32(m_g.dw(DXVEC + 8)) << 4;
            u16 lim = static_cast<u16>(z0 << 4);
            u16 dzh = static_cast<u16>(dz >> 16);
            if (dzh > lim) {
                SW(Z0FACTOR, static_cast<i16>(((static_cast<u32>(lim) << 16) / dzh) >> 1));
                reducecoords();
            }
        }
    }
    return false;
}

bool F3dVec::disteval(u16 si) {
    rel3d(si);
    const u16 cx = m_range;
    auto sq = [](u16 a) { u16 v = static_cast<u16>(abs16(static_cast<i16>(a))); return static_cast<u32>(v) * v; };
    u32 sum = 0;
    if (cx >> 8) {
        SB(si + 18, 3);
        return true;
    } else if (cx & 0xF0) {
        SB(si + 18, 2);
        for (int k = 0; k < 3; k++) sum += sq(static_cast<u16>(static_cast<u32>(m_g.dw(DXVEC + 4 * k)) >> 8));
    } else if (cx & 0xFF) {
        SB(si + 18, 1);
        for (int k = 0; k < 3; k++) sum += sq(static_cast<u16>(static_cast<u32>(m_g.dw(DXVEC + 4 * k)) >> 4));
    } else {
        SB(si + 18, 0);
        for (int k = 0; k < 3; k++) sum += sq(static_cast<u16>(W(DXVEC + 4 * k)));
    }
    m_g.setDw(si + 20, static_cast<i32>(sum));
    return false;
}

void F3dVec::d3dobj(u16 si, u16 dx) {
    if (W(si + 24) == 0) return;   // object type 0 = not drawn
    m_cwin = dx;
    if (disteval(si)) return;
    const u32 dist = static_cast<u32>(m_g.dw(si + 20));
    auto bkpt = [&](u32 o) { return static_cast<u32>(m_g.dw(si + o)); };
    auto toXvec = [&](int shift) {
        for (int k = 0; k < 3; k++)
            SW(XVEC + 2 * k, static_cast<i16>(static_cast<u16>(static_cast<u32>(m_g.dw(DXVEC + 4 * k)) >> shift)));
    };
    const u8 code = B(si + 18);
    if (code == 0) {
        if (dist > bkpt(28)) {           // too far for the near shape: force medium
            m_rdf = 4;
            m_objptr = static_cast<u16>(W(si + 42));
            toXvec(m_rdf + 1);
        } else {
            m_rdf = 0;
            toXvec(1);
            m_objptr = static_cast<u16>(W(si + 40));
        }
    } else {
        m_rdf = 4;
        m_objptr = static_cast<u16>(W(si + 42));
        bool far = true;
        if (code == 1) far = dist > bkpt(32);
        else if (dist > bkpt(36)) return;
        if (far) {
            if (W(Z0) != 0 && !(W(si + 24) & 0x100) && W(OX + 10) > W(Z0)) return; // map mode: walls only
            m_rdf = 8;
            m_objptr = static_cast<u16>(W(si + 44));
        }
        toXvec(m_rdf + 1);
    }
    // rotate the object by its own angles only while it is close enough
    if (static_cast<int8_t>(B(si + 18) - B(si + 26)) < 0) relrot(si);
    else nrelrot(si);
    drrobj(m_objptr, OMAT, m_cwin);
    m_rdf = 0;
}

// Positions the horizon object 1300 units ahead at eye level, its line
// perpendicular to the view direction
void F3dVec::modhr() {
    const u16 si = static_cast<u16>(W(NOROTPTR));
    const i16 a13 = hmul(1300, W(si + 6));
    SW(Z0A13, a13);
    m_g.setDw(HROBJ + 4, static_cast<i32>(static_cast<u32>(a13) + static_cast<u32>(m_g.dw(OX + 4))));
    const i16 a33 = hmul(1300, W(si));
    SW(Z0A33, a33);
    m_g.setDw(HROBJ, static_cast<i32>(static_cast<u32>(a33) + static_cast<u32>(m_g.dw(OX))));
    m_g.setDw(HROBJ + 8, m_g.dw(OX + 8));
    SW(HRWR1 + 10, a13);
    SW(HRWR1 + 4, neg16(a13));
    SW(HRWR1 + 6, a33);
    SW(HRWR1 + 12, neg16(a33));
    int8_t ah = static_cast<int8_t>(static_cast<u16>(W(VIEWKLUDGE)) >> 8);
    ah = static_cast<int8_t>(ah >> 1);
    m_dvflg = static_cast<u8>(-ah);
}

// =====================================================================
// Object tables (kept sorted far to near)
// =====================================================================

void F3dVec::tbadd(u16 si, u16 bx) {
    m_tbstrt = si;
    u16 cx = static_cast<u16>(W(si));
    si += 4;
    u8 al = B(bx + 18);
    if (static_cast<int8_t>(al) < 0) {
        al = 0;
        SB(bx + 18, 0);
    }
    SB(bx + 18, B(bx + 18) & 3);
    m_odcode = static_cast<u16>((m_odcode & 0xFF00) | al);
    const u32 dist = static_cast<u32>(m_g.dw(bx + 20));
    for (;; si += 2) {
        if (cx == 0) break;   // append
        const u16 di = static_cast<u16>(W(si));
        const u8 other = B(di + 18);
        bool ins;
        if (al != other) ins = al > other;
        else if ((dist >> 16) != static_cast<u16>(W(di + 22))) ins = (dist >> 16) > static_cast<u16>(W(di + 22));
        else {
            const u32 od = static_cast<u32>(m_g.dw(di + 20));
            ins = dist >= od;
            if (dist == od && bx == di) cecode = 28;   // duplicate entry
        }
        if (ins) {
            // make room: shift the rest of the table up one slot
            for (u16 k = cx; k; k--) SW(si + 2 * k, W(si + 2 * k - 2));
            break;
        }
        if (static_cast<i16>(--cx) < 0) { cecode = 14; return; }
    }
    SW(si, static_cast<i16>(bx));
    SW(m_tbstrt, add16(W(m_tbstrt), 1));
    if (!sf16(W(m_tbstrt), W(m_tbstrt + 2))) cecode = 10;   // table full
}

void F3dVec::tbsort(u16 si) {
    m_tbstrt = si;
    for (;;) {
        bool swapped = false;
        si = m_tbstrt;
        u16 cx = static_cast<u16>(W(si));
        si += 4;
        if (cx == 0 || cx == 1) return;
        cx--;
        u16 bx = static_cast<u16>(W(si));
        u8 od = B(bx + 18);
        u32 dist = static_cast<u32>(m_g.dw(bx + 20));
        do {
            bx = static_cast<u16>(W(si + 2));
            const u8 other = B(bx + 18);
            bool swap;
            if (od != other) swap = od < other;
            else if ((dist >> 16) != static_cast<u16>(W(bx + 22))) swap = (dist >> 16) < static_cast<u16>(W(bx + 22));
            else swap = dist < static_cast<u32>(m_g.dw(bx + 20));
            if (swap) {
                bx = static_cast<u16>(W(si));
                u16 t = static_cast<u16>(W(si + 2));
                SW(si + 2, static_cast<i16>(bx));
                bx = t;
                SW(si, static_cast<i16>(bx));
                swapped = true;
            }
            od = B(bx + 18);
            dist = static_cast<u32>(m_g.dw(bx + 20));
            si += 2;
        } while (--cx);
        if (!swapped) return;
    }
}

void F3dVec::tbrmv(u16 si, u16 bx) {
    m_tbstrt = si;
    u16 cx = static_cast<u16>(W(si));
    si += 4;
    for (; cx; cx--, si += 2) {
        if (static_cast<u16>(W(si)) == bx) {
            SB(bx + 18, 0xFF);   // flag as really removed
            movsw(si, si + 2, cx - 1);
            SW(m_tbstrt, sub16(W(m_tbstrt), 1));
            return;
        }
    }
    cecode = 11;
}

bool F3dVec::checkforrunway(u16 si) {
    if (W(si + 8) | W(si + 10)) return false;   // not on the ground
    const u16 type = static_cast<u16>(W(si + 72));
    return (W(EXTENDEDTYPETBL + 2 * type) & 16) != 0;
}

void F3dVec::drawnear() {
    m_objcount = 0;
    const u16 tbl = static_cast<u16>(W(TBLIST));

    // runway pieces go down first, under everything else
    u16 bx = tbl + 4;
    for (u16 cx = static_cast<u16>(W(tbl)); cx; cx--, bx += 2) {
        const u16 si = static_cast<u16>(W(bx));
        if (W(si + 24) == 0) continue;
        m_objcount++;
        if (!checkforrunway(si)) continue;
        d3dobj(si, 0);
        if (B(EJECTFLAG)) qwalk(si);
        qcrash(si);
    }
    if (m_objcount > static_cast<u16>(W(MAXOBJCOUNT))) SW(MAXOBJCOUNT, static_cast<i16>(m_objcount));

    // then everything else in table order (far to near)
    m_tbstrt = tbl;
    u16 cx = static_cast<u16>(W(tbl));
    bx = tbl + 4;
    if (!cx) return;
    do {
        const u16 si = static_cast<u16>(W(bx));
        m_odcode = B(si + 18);
        qtimed(si);
        bool draw = true;
        if (cx > 1) {
            if (W(si + 24) == 0) draw = false;
            else qcollis(si);
        }
        if (draw && checkforrunway(si)) {
            m_rdf = B(si + 18);   // already drawn in the runway pass
        } else {
            if (draw) {
                d3dobj(si, 0);
                if (B(EJECTFLAG)) qwalk(si);
            }
            qcrash(si);
        }
        if (W(si + 24) == 0) {
            const u16 keep = m_tbstrt;
            tbrmv(m_tbstrt, si);   // the next entry slides into this slot
            m_tbstrt = keep;
        } else {
            bx += 2;
        }
    } while (--cx);
    tbsort(m_tbstrt);
}

// =====================================================================
// Page borders, page flip, palette
// =====================================================================

void F3dVec::epage() {
    const int s = m_scr.scale();
    // Byte-addressed fills, as the EGA sees them: di is a byte offset into the page
    auto bar = [&](int di, int rows, u8 c) {   // thckbar: one byte wide
        m_scr.fillRect((di % BPL) * 8 * s, (di / BPL) * s, 8 * s, rows * s, c);
    };
    auto run = [&](int di, int bytes, u8 c) {  // rep stosw over whole rows
        for (; bytes > 0;) {
            int row = di / BPL, col = di % BPL, n = std::min(bytes, BPL - col);
            m_scr.fillRect(col * 8 * s, row * s, n * 8 * s, s, c);
            di += n;
            bytes -= n;
        }
    };
    auto box = [&](int di, int words, int rows, u8 c) {   // fillbx
        for (int r = 0; r < rows; r++) run(di + r * BPL, words * 2, c);
    };

    const u8 c = B(BRDRCLR);
    const u16 f = static_cast<u16>(W(PTRGWINF));
    const int ymindi = W(f + 20), ysiz = W(f + 10), ymaxdi = W(f + 22);
    bar(ymindi - 1, ysiz, c);
    bar(ymindi - 1 + BPL - 1, ysiz, c);
    if (B(JUMPFLAG)) {
        // jump charge meter in the right border, magenta
        u16 len = static_cast<u16>((static_cast<u32>(static_cast<u16>(ysiz)) * static_cast<u16>(W(JUMPVALUE))) / 1024);
        int start = static_cast<u16>(ysiz) - len;
        bar(ymindi - 1 + BPL - 1 + start * BPL, len, 5);
    }
    run(0, BRTHK * BPL * 2, c);
    run(ymaxdi, BRTHK * BPL * 2, c);
    if (W(PTRGWINT)) {
        const int gtmnb = GTYMIN * BPL + GTXMIN / 8;
        const int words = (GTXMAX - GTXMIN + 1) / 16 + 1;
        box(gtmnb - BRTHK * BPL - 1 - BPL, words, BRTHK + 1, c);
        box((VYSIZ - BRTHK - 1) * BPL + GTXMIN / 8 - BPL - 1, words, BRTHK + 1, c);
        bar(gtmnb - 1, GTYMAX - GTYMIN + 1, c);
        bar(gtmnb + (GTXMAX - GTXMIN + 1) / 8, GTYMAX - GTYMIN + 1, c);
    }
}

void F3dVec::flipage() {
    if (W(BRDRCNT)) {   // "madness": flash the border
        SB(BRDRCLR, B(BRDRCLR) ^ 0x0b);
        SW(BRDRCNT, sub16(W(BRDRCNT), 1));
    }
    u8 bh = 0, bl = 0;
    bool shake = false;
    if (B(ROCKETFLAG)) {
        bh = 80;
        shake = B(EJECTFLAG) == 0;
    } else if (B(EJECTFLAG)) {
        SB(BRDRCLR, 7);
        SW(BRDRCNT, 0);
        SB(SHAKECNT, 0);
    } else if (B(SHAKECNT)) {
        SB(SHAKECNT, B(SHAKECNT) - 1);
        bh = 241;
        shake = true;
    }
    if (shake) bl = (W(FRMNUM) & 1) ? bh : 0;
    // qstopblam (sound) belongs to the audio phase
    SB(PGFLG, B(PGFLG) ^ 0xFF);
    SW(VBOFF, B(PGFLG) ? 0 : 0x7e00);
    m_scr.flip();
    // the start address moves by bl bytes: whole rows down, then 8-pixel steps
    const int s = m_scr.scale();
    m_panY = (bl / BPL) * s;
    m_panX = (bl % BPL) * 8 * s;
}

void F3dVec::readcolors() {
    m_pal = EgaPalette::defaults();
    for (int i = 0; i < 16; i++) {
        u8 a = m_pal.attr[i];
        SB(COLORS + 4 * i, a);
        for (int c = 0; c < 3; c++) SB(COLORS + 4 * i + 1 + c, m_pal.dac[a & 63][c]);
    }
    unpackbits();
    setskygnd();
}

// colorbits: 6 colors x 6 bits packed into 40 bits (sky rgb, then ground rgb)
void F3dVec::unpackbits() {
    uint64_t v = 0;
    for (int i = 0; i < 5; i++) v = (v << 8) | B(COLORBITS + i);
    if (!v) return;
    const u32 dst[6] = {GROUNDCOLORS + 2, GROUNDCOLORS + 1, GROUNDCOLORS, SKYCOLORS + 2, SKYCOLORS + 1, SKYCOLORS};
    for (int i = 0; i < 6; i++) {
        SB(dst[i], static_cast<u8>((v >> 2) & 63));
        v >>= 6;
    }
}

void F3dVec::packbits() {
    const u32 src[6] = {SKYCOLORS, SKYCOLORS + 1, SKYCOLORS + 2, GROUNDCOLORS, GROUNDCOLORS + 1, GROUNDCOLORS + 2};
    uint64_t v = 0;
    for (int i = 0; i < 6; i++) v = (v << 6) | (B(src[i]) & 63);
    v <<= 2;
    for (int i = 4; i >= 0; i--, v >>= 8) SB(COLORBITS + i, static_cast<u8>(v));
}

void F3dVec::setskygnd() {
    for (int c = 0; c < 3; c++) {
        SB(COLORS + 4 * 2 + 1 + c, B(GROUNDCOLORS + c));
        SB(COLORS + 4 * 11 + 1 + c, B(SKYCOLORS + c));
    }
}

void F3dVec::setcolors(u8 mul) {
    for (int i = 0; i < 16; i++) {
        u8 idx = B(COLORS + 4 * i) & 63;
        for (int c = 0; c < 3; c++)
            m_pal.dac[idx][c] = static_cast<u8>((B(COLORS + 4 * i + 1 + c) * mul) >> 8);
    }
}

void F3dVec::distcolors() {
    for (int i = 0; i < 16; i++) {
        u8 idx = B(COLORS + 4 * i) & 63;
        for (int c = 0; c < 3; c++) {
            // workonclr: add the bias; past 63 the value runs back down
            u8 al = static_cast<u8>(B(COLORS + 4 * i + 1 + c) + B(HALUCLRS + 1 + 2 * c));
            al = (al & 0x40) ? static_cast<u8>(0x3f - (al & 0x3f)) : static_cast<u8>(al & 0x3f);
            SB(RESCOLORS + 4 * i + 1 + c, al);
            m_pal.dac[idx][c] = al;
        }
    }
}

} // namespace corncob::engine
