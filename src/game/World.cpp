// World.cpp - Port of the universe code: F3DVEC.ASM tiles/memseg, 3.ASM file readers
//
// "js/jns" after a CMP/SUB tests the sign of the 16-bit difference; "jl/jg" are
// real signed compares; "jb/ja" are unsigned. The carry flag the ASM procs return
// is the bool result of the matching method (true = carry set).

#include "World.hpp"
#include "DGroup.hpp"
#include "core/AsmMath.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace corncob::game {

namespace d = corncob::dgroup;
using asmx::sf16;

namespace {

// F3DVEC.ASM / 3.ASM / DEFOBJS.ASM / STRTEXP.ASM labels
constexpr u32 CURTILEX = d::f3dvec::curtilex, CURTILEY = d::f3dvec::curtiley;
constexpr u32 UNIVERSEX = d::f3dvec::universex, UNIVERSEY = d::f3dvec::universey;
constexpr u32 CTX = d::f3dvec::ctx, CTY = d::f3dvec::cty;
constexpr u32 CTILES = d::f3dvec::ctiles, CTILEOFFS = d::f3dvec::ctileoffs;
constexpr u32 CTSTATUS = d::f3dvec::ctstatus, BOGUS = d::f3dvec::bogus;
constexpr u32 TILEPTR = d::f3dvec::tileptr, TILESEG = d::f3dvec::tileseg, TOBJCOUNT = d::f3dvec::tobjcount;
constexpr u32 EXPTR = d::f3dvec::exptr, FDEXPOBJ = d::f3dvec::fdexpobj, MEMOBJBUF = d::f3dvec::memobjbuf;
constexpr u32 HEADER = d::f3dvec::header, HEADERX = d::f3dvec::headerx, HEADERY = d::f3dvec::headery;
constexpr u32 HEADERSZ = d::f3dvec::headersz, TBLIST = d::f3dvec::tblist;
constexpr u32 TEMPLATETABLE = d::defobjs::templatetable, NTEMPLATES = d::defobjs::ntemplates;
constexpr u32 EXTENDEDTYPETBL = d::defobjs::extendedtypetbl;
constexpr u32 EXPS = d::strtexp::exps;
constexpr u32 OX = d::m3::ox, KBUF = d::m3::kbuf, STARTCOORDS = d::m3::startcoords;
constexpr u32 DRAWMEMFLAG = d::m3::drawmemflag;
constexpr u32 NPLACED = d::defobjs::nplaced;
constexpr u32 ACTIONFLAG = d::m3::actionflag, READFILEFLAG = d::m3::readfileflag;

constexpr int NPARALINE = World::NPARATILE * World::NTILESX;

// sas.lib's str_skipw / dec_to_dwords (not in the source; inferred from how
// readunivparams and getstartfile use them). Skip blanks, then read an optionally
// signed decimal number; carry (false result) when there are no digits.
struct Scan {
    const GameState& g;
    u32 si;
    void skipw() {
        while (g.b(si) != 0 && g.b(si) <= ' ') si++;
    }
    bool decToDwords(int32_t& value) {
        value = 0;
        bool neg = false, any = false;
        if (g.b(si) == '-') { neg = true; si++; }
        uint32_t v = 0;
        while (g.b(si) >= '0' && g.b(si) <= '9') {
            v = v * 10 + (g.b(si) - '0');
            any = true;
            si++;
        }
        value = static_cast<int32_t>(neg ? 0u - v : v);
        return any;
    }
};

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    uint8_t buf[4096];
    out.clear();
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return true;
}

} // namespace

World::World(GameState& gs, engine::F3dVec& eng)
    : m_g(gs), m_eng(eng), m_seg(static_cast<size_t>(MEMSEGSKIP + NTILESX * NTILESY * NPARATILE) * 16, 0) {}

// ---- far segment access ----

i16 World::segW(u32 seg, u32 off) const {
    const size_t i = static_cast<size_t>(seg) * 16 + off;
    if (i + 1 >= m_seg.size()) return 0;
    return static_cast<i16>(m_seg[i] | (m_seg[i + 1] << 8));
}
void World::setSegW(u32 seg, u32 off, i16 v) {
    const size_t i = static_cast<size_t>(seg) * 16 + off;
    if (i + 1 >= m_seg.size()) return;
    m_seg[i] = static_cast<u8>(v);
    m_seg[i + 1] = static_cast<u8>(static_cast<u16>(v) >> 8);
}
u8 World::segB(u32 seg, u32 off) const {
    const size_t i = static_cast<size_t>(seg) * 16 + off;
    return i < m_seg.size() ? m_seg[i] : 0;
}
void World::setSegB(u32 seg, u32 off, u8 v) {
    const size_t i = static_cast<size_t>(seg) * 16 + off;
    if (i < m_seg.size()) m_seg[i] = v;
}

// ---- 3.ASM: theater files ----

// 3.ASM: readunivparams
void World::readunivparams() {
    Scan s{m_g, KBUF + 1};   // skip over the 'X'
    int32_t v = 0;
    s.skipw();
    s.decToDwords(v);
    SW(d::m3::sunangle, static_cast<i16>(v));
    s.skipw();
    s.decToDwords(v);
    SW(d::defobjs::sunsz1, static_cast<i16>(v));
    SW(d::defobjs::sunsz2, static_cast<i16>(v + 5));
    s.skipw();
    s.decToDwords(v);
    SB(d::defobjs::sunclr, static_cast<u8>(v & 0x0F));
    // gravity scale factor, kept in [-100,-15]
    s.skipw();
    s.decToDwords(v);
    i16 ax = static_cast<i16>(v);
    if (!(ax < -15)) ax = -15;
    if (!(ax > -100)) ax = -100;
    SW(d::m3::grvsf, ax);
    // air density at ground level
    s.skipw();
    s.decToDwords(v);
    ax = static_cast<i16>(v);
    if (!(ax < 0) && !(ax < 1000) && static_cast<u16>(ax) <= 10000) {
        SW(d::m3::rho0, ax);
        SW(d::m3::rho02, static_cast<i16>(static_cast<u16>(ax * 2) / 3));
    }
    s.skipw();
    s.decToDwords(v);
    SB(d::m3::starsflag, static_cast<u8>(v & 1));
    // rho0z, and the altitude where the stars start (shareware: 30/50 of it)
    s.skipw();
    const bool ok = s.decToDwords(v);
    ax = static_cast<i16>(v);
    if (ok && !(ax < 0) && static_cast<u16>(ax) >= 256) {
        SW(d::m3::rho0z, ax);
        SW(d::m3::starheight, static_cast<i16>(static_cast<u32>(static_cast<u16>(ax)) * 30 / 50));
    }
}

// 3.ASM: getstartfile
bool World::getstartfile(const std::string& path) {
    std::vector<uint8_t> f;
    if (!readFile(path, f)) return false;   // forget it if no file
    for (size_t i = 0; i < 80 && i < f.size(); i++) SB(KBUF + static_cast<u32>(i), f[i]);
    Scan s{m_g, KBUF};
    u32 di = d::m3::tempstart;
    int32_t v = 0;
    for (int k = 0; k < 3; k++) {   // x, y, z as 32-bit
        s.skipw();
        s.decToDwords(v);
        m_g.setDw(di, v);
        di += 4;
    }
    s.skipw();
    s.decToDwords(v);
    SW(di, static_cast<i16>(v));   // yaw angle for the plane's position
    SB(d::m3::newmissflag, 0xFF);
    return true;
}

// 3.ASM: readuniv. The file is the whole of 3univ.dat; false = the original's
// "rerrorres3" (a tile header that does not match).
bool World::readuniv(const std::vector<uint8_t>& file) {
    size_t pos = 0;
    auto take = [&](u32 dstDg, size_t n) {   // read_h into DGROUP
        for (size_t i = 0; i < n; i++) SB(dstDg + static_cast<u32>(i), pos + i < file.size() ? file[pos + i] : 0);
        pos += n;
    };
    for (int y = 0; y < NTILESY; y++) {
        for (int x = 0; x < NTILESX; x++) {
            if (pos + NHEADERBYTES > file.size()) return false;
            take(HEADER, NHEADERBYTES);
            SW(CURTILEX, static_cast<i16>(x));
            SW(CURTILEY, static_cast<i16>(y));
            if (W(HEADERX) != x || W(HEADERY) != y) return false;
            if (!getheader()) return false;
            const u16 seg = gettileadr();
            const size_t n = static_cast<size_t>(static_cast<u16>(W(HEADERSZ))) * NBYTEOBJ;
            for (size_t i = 0; i < n; i++)   // a short read leaves the tile as it was
                if (pos + i < file.size()) setSegB(seg, static_cast<u32>(i), file[pos + i]);
            pos += n;
            // newthtflag (shareware): the template index is stored xor'ed with the x and y words
            for (u16 k = 0, di = 0; k < static_cast<u16>(W(HEADERSZ)); k++, di += NBYTEOBJ)
                setSegW(seg, di + 21u, static_cast<i16>(segW(seg, di + 21u) ^ segW(seg, di + 4u) ^ segW(seg, di + 2u)));
        }
    }
    for (u32 i = 0; i < 12; i++) SB(STARTCOORDS + i, pos + i < file.size() ? file[pos + i] : 0);
    return true;
}

// 3.ASM: readunivfile
bool World::readunivfile(const std::string& towersPath, const std::string& univPath) {
    std::vector<uint8_t> f;
    if (readFile(towersPath, f)) {
        for (size_t i = 0; i < 80 && i < f.size(); i++) SB(KBUF + static_cast<u32>(i), f[i]);
        if (B(KBUF) == 'X') readunivparams();
    }
    if (!readFile(univPath, f)) return false;
    return readuniv(f);
}

// ---- F3DVEC.ASM: tiles ----

// F3DVEC.ASM: gettileadr (the memseg base is 0 here)
u16 World::gettileadr() {
    const u16 ax = static_cast<u16>(static_cast<u16>(W(CURTILEY)) * NPARALINE);
    const u16 bx = ax;
    u16 r = static_cast<u16>(static_cast<u16>(W(CURTILEX)) * NPARATILE);
    r = static_cast<u16>(r + bx + MEMSEGSKIP);
    return r;
}

// F3DVEC.ASM: qintile
bool World::qintile(u16 si) {
    i16 dx = W(si + 2u);
    i16 ax = static_cast<i16>(static_cast<i16>(W(CURTILEX) << 5) + W(UNIVERSEX));
    dx = static_cast<i16>(dx - ax);
    if (sf16(dx, 0)) return true;
    dx = static_cast<i16>(dx - 32);
    if (!sf16(dx, 0)) return true;
    dx = W(si + 6u);
    ax = static_cast<i16>(static_cast<i16>(W(CURTILEY) << 5) + W(UNIVERSEY));
    dx = static_cast<i16>(dx - ax);
    if (sf16(dx, 0)) return true;
    dx = static_cast<i16>(dx - 32);
    if (!sf16(dx, 0)) return true;
    return false;
}

// F3DVEC.ASM: updatect
void World::updatect() {
    i16 ax = static_cast<i16>(W(OX + 2) - W(UNIVERSEX) + TILESZ / 2);
    SW(CTX, static_cast<i16>(ax / TILESZ));
    ax = static_cast<i16>(W(OX + 4 + 2) - W(UNIVERSEY) + TILESZ / 2);
    SW(CTY, static_cast<i16>(ax / TILESZ));
}

// F3DVEC.ASM: qtilefar
bool World::qtilefar() {
    if (static_cast<u16>(W(CTX) - W(CURTILEX)) & 0xFFFE) return true;
    if (static_cast<u16>(W(CTY) - W(CURTILEY)) & 0xFFFE) return true;
    return false;
}

// F3DVEC.ASM: getctstatusptr
u16 World::getctstatusptr() {
    const i16 x = W(CURTILEX), y = W(CURTILEY);
    if (x < 0 || x >= NTILESX || y < 0 || y >= NTILESY) {
        m_eng.cecode = 42;   // report violation
        return static_cast<u16>(BOGUS);
    }
    return static_cast<u16>(y * NTILESX + x + CTSTATUS);
}

// F3DVEC.ASM: farmthem
void World::farmthem() {
    u16 si = static_cast<u16>(CTILES);
    for (int cx = NDEPTH; cx; cx--, si += 4) {
        const i16 x = W(si);
        SW(CURTILEX, x);
        if (x < 0 || x >= NTILESX) continue;
        const i16 y = W(si + 2u);
        SW(CURTILEY, y);
        if (y < 0 || y >= NTILESY) continue;
        if (!qtilefar()) continue;
        farmmemseg();
        SB(getctstatusptr(), 0);
    }
}

// F3DVEC.ASM: changethem
void World::changethem() {
    for (u16 si = 0, cx = NDEPTH; cx; cx--, si += 4) {
        SW(CTILES + si, static_cast<i16>(W(CTX) + W(CTILEOFFS + si)));
        SW(CTILES + si + 2u, static_cast<i16>(W(CTY) + W(CTILEOFFS + si + 2u)));
    }
}

// F3DVEC.ASM: bringthem
void World::bringthem() {
    u16 si = static_cast<u16>(CTILES);
    for (int cx = NDEPTH; cx; cx--, si += 4) {
        const i16 x = W(si);
        SW(CURTILEX, x);
        if (x < 0 || x >= NTILESX) continue;
        const i16 y = W(si + 2u);
        SW(CURTILEY, y);
        if (y < 0 || y >= NTILESY) continue;
        const u16 bx = getctstatusptr();
        if (B(bx)) continue;   // already read in
        bringmemseg();
        SB(bx, 0xFF);
    }
}

// F3DVEC.ASM: dotiles
void World::dotiles() {
    updatect();
    farmthem();
    changethem();
    bringthem();
}

// F3DVEC.ASM: decodeobj (tile record at seg:di -> object at [si])
bool World::decodeobj(u16 seg, u16 di, u16 si) {
    i16 ax = segW(seg, di + 11u);
    if (ax == 0) return true;   // no object there
    SW(si + 24u, ax);
    const i16 bx = segW(seg, di + 21u);   // template index
    SW(si + 72u, bx);
    if (static_cast<u16>(bx) > static_cast<u16>(W(NTEMPLATES))) {
        m_eng.cecode = 39;
        return true;
    }
    // start from the template, so fields the tile does not store keep their defaults
    const u16 tmpl = static_cast<u16>(W(TEMPLATETABLE + 2u * static_cast<u16>(bx)));
    std::memmove(m_g.raw() + si, m_g.raw() + tmpl, NBYTEXP);

    const u16 curx = static_cast<u16>(W(CURTILEX)), cury = static_cast<u16>(W(CURTILEY));
    const u16 bits = static_cast<u16>(segW(seg, di));
    SW(si, segW(seg, di + 2u));
    SW(si + 2u, static_cast<i16>(curx * TILESZ + W(UNIVERSEX) + (bits & (TILESZ - 1))));
    SW(si + 4u, segW(seg, di + 4u));
    SW(si + 6u, static_cast<i16>(cury * TILESZ + W(UNIVERSEY) + ((bits >> 5) & (TILESZ - 1))));
    SW(si + 8u, segW(seg, di + 6u));
    SW(si + 10u, static_cast<i16>((bits >> 10) & (TILESZ - 1)));
    SW(si + 12u, static_cast<i16>(segB(seg, di + 8u) << 8));    // yaw
    SW(si + 14u, static_cast<i16>(segB(seg, di + 9u) << 8));    // pitch
    SW(si + 16u, static_cast<i16>(segB(seg, di + 10u) << 8));   // roll
    SW(si + 46u, segW(seg, di + 13u));   // velocity
    SW(si + 48u, segW(seg, di + 15u));
    SW(si + 50u, segW(seg, di + 17u));
    SW(si + 52u, segW(seg, di + 19u));   // timed object time value
    SW(si + 24u, segW(seg, di + 11u));   // object type
    return false;
}

// F3DVEC.ASM: encodeobj (object at [si] -> tile record at seg:di)
bool World::encodeobj(u16 si, u16 seg, u16 di) {
    setSegW(seg, di + 2u, W(si));
    const u16 curx = static_cast<u16>(W(CURTILEX)), cury = static_cast<u16>(W(CURTILEY));
    u16 bx = static_cast<u16>(W(si + 2u) - (curx * TILESZ + W(UNIVERSEX)));
    bx &= TILESZ - 1;
    setSegW(seg, di, static_cast<i16>(bx));
    setSegW(seg, di + 4u, W(si + 4u));
    bx = static_cast<u16>(W(si + 6u) - (cury * TILESZ + W(UNIVERSEY)));
    bx &= TILESZ - 1;
    bx = static_cast<u16>((bx << 5) | (bx >> 11));   // rol 5
    setSegW(seg, di, static_cast<i16>(segW(seg, di) | bx));
    setSegW(seg, di + 6u, W(si + 8u));
    bx = static_cast<u16>(W(si + 10u)) & (TILESZ - 1);
    bx = static_cast<u16>((bx << 10) | (bx >> 6));   // rol 10
    setSegW(seg, di, static_cast<i16>(segW(seg, di) | bx));
    setSegB(seg, di + 8u, static_cast<u8>(static_cast<u16>(W(si + 12u)) >> 8));
    setSegB(seg, di + 9u, static_cast<u8>(static_cast<u16>(W(si + 14u)) >> 8));
    setSegB(seg, di + 10u, static_cast<u8>(static_cast<u16>(W(si + 16u)) >> 8));
    setSegW(seg, di + 11u, W(si + 24u));
    setSegW(seg, di + 13u, W(si + 46u));
    setSegW(seg, di + 15u, W(si + 48u));
    setSegW(seg, di + 17u, W(si + 50u));
    setSegW(seg, di + 19u, W(si + 52u));
    const u16 type = static_cast<u16>(W(si + 72u));
    if (type > static_cast<u16>(W(NTEMPLATES))) {
        m_eng.cecode = 40;
        return true;
    }
    setSegW(seg, di + 21u, static_cast<i16>(type));
    return false;
}

// F3DVEC.ASM: farmmemseg - writes the memseg objects inside the current tile
// back to the tile (the carry flag is always set on return, as in the original)
bool World::farmmemseg() {
    const u16 seg = gettileadr();
    u16 tileptr = 0;
    SW(TILEPTR, 0);
    SW(TILESEG, static_cast<i16>(seg));
    SW(TOBJCOUNT, 0);
    u16 si = 0;
    for (int cx = NMAXMEDFAR; cx; cx--) {
        if (segW(0, si + 24u) != 0) {   // slot in use
            frommemseg(si, MEMOBJBUF);
            if (!qintile(static_cast<u16>(MEMOBJBUF))) {
                SW(TOBJCOUNT, static_cast<i16>(W(TOBJCOUNT) + 1));
                if (static_cast<u16>(W(TOBJCOUNT)) >= NOBJTILE - 1) {   // tile is full
                    SW(TOBJCOUNT, static_cast<i16>(W(TOBJCOUNT) - 1));
                } else {
                    setSegW(seg, tileptr + NBYTEOBJ + 11u, 0);   // zero the next object
                    if (encodeobj(static_cast<u16>(MEMOBJBUF), seg, tileptr)) {
                        m_eng.cecode = 41;
                    } else {
                        tileptr = static_cast<u16>(tileptr + NBYTEOBJ);
                        SW(TILEPTR, static_cast<i16>(tileptr));
                        setSegW(0, si + 24u, 0);   // remove the object from the table
                    }
                }
            }
        }
        si = static_cast<u16>(si + NBYTEXP);
        if (si > MSEPTRMX) si = 0;
    }
    return true;
}

// F3DVEC.ASM: bringmemseg - reads the current tile's objects into free memseg slots
bool World::bringmemseg() {
    const u16 seg = gettileadr();
    u16 tileptr = 0;
    SW(TILEPTR, 0);
    SW(TILESEG, static_cast<i16>(seg));
    SW(TOBJCOUNT, 0);
    u16 si = 0;
    for (int cx = NMAXMEDFAR; cx; cx--) {
        if (segW(0, si + 24u) == 0) {   // free slot
            if (static_cast<u16>(W(TOBJCOUNT)) >= NOBJTILE) return false;
            if (decodeobj(seg, tileptr, static_cast<u16>(MEMOBJBUF))) return false;
            tileptr = static_cast<u16>(tileptr + NBYTEOBJ);
            SW(TILEPTR, static_cast<i16>(tileptr));
            SW(TOBJCOUNT, static_cast<i16>(W(TOBJCOUNT) + 1));
            for (int k = 0; k < NBYTEXP; k++) setSegB(0, si + static_cast<u32>(k), B(MEMOBJBUF + k));
        }
        si = static_cast<u16>(si + NBYTEXP);
        if (si > MSEPTRMX) si = 0;
    }
    return true;
}

// F3DVEC.ASM: gettilesize - counts the objects of the tile at seg into headersz
void World::gettilesize(u16 seg, u16& di) {
    SW(HEADERSZ, 0);
    for (int cx = NOBJTILE - 1; cx; cx--) {
        if (segW(seg, di + 11u) == 0) return;
        SW(HEADERSZ, static_cast<i16>(W(HEADERSZ) + 1));
        di = static_cast<u16>(di + NBYTEOBJ);
    }
}

// F3DVEC.ASM: fillheader
void World::fillheader() {
    SW(HEADERX, W(CURTILEX));
    SW(HEADERY, W(CURTILEY));
    u16 di = 0;
    gettilesize(gettileadr(), di);
}

// F3DVEC.ASM: getheader - only the second word of the "Pie",0 tag is compared
bool World::getheader() {
    if (W(HEADER + 2) != 0x0065) return false;
    SW(CURTILEX, W(HEADERX));
    SW(CURTILEY, W(HEADERY));
    return true;
}

// F3DVEC.ASM: flushtile - encodes the near table's objects into the current tile
void World::flushtile() {
    const u16 tbstrt = static_cast<u16>(W(TBLIST));
    u16 cx = static_cast<u16>(W(tbstrt));
    u16 bx = static_cast<u16>(tbstrt + 4);
    for (; cx; cx--, bx += 2) {
        const u16 si = static_cast<u16>(W(bx));
        if (W(si + 24u) == 0) continue;   // object missing
        const u16 ext = static_cast<u16>(W(EXTENDEDTYPETBL + 2u * static_cast<u16>(W(si + 72u))));
        if (ext & 1) continue;            // these are not saved
        if (static_cast<u16>(W(TOBJCOUNT)) >= NOBJTILE - 1) continue;   // tile is full
        SW(TOBJCOUNT, static_cast<i16>(W(TOBJCOUNT) + 1));
        if (qintile(si)) continue;        // not in this tile
        const u16 seg = static_cast<u16>(W(TILESEG));
        const u16 tileptr = static_cast<u16>(W(TILEPTR));
        setSegW(seg, tileptr + NBYTEOBJ + 11u, 0);   // clear the object after this
        encodeobj(si, seg, tileptr);
        SW(TILEPTR, static_cast<i16>(tileptr + NBYTEOBJ));
        SW(si + 24u, 0);                  // remove from the timed object table
        SB(si + 18u, 0xFF);               // mark as fully flushed
    }
}

// F3DVEC.ASM: flushtiles - all object tables back into the tiles
void World::flushtiles() {
    updatect();
    changethem();
    u16 si = static_cast<u16>(CTILES);
    for (int cx = NDEPTH; cx; cx--, si += 4) {
        const i16 x = W(si);
        SW(CURTILEX, x);
        if (x < 0 || x >= NTILESX) continue;
        const i16 y = W(si + 2u);
        SW(CURTILEY, y);
        if (y < 0 || y >= NTILESY) continue;
        farmmemseg();   // flush the memseg med table (leaves tileptr after its objects)
        flushtile();
        SB(getctstatusptr(), 0);
    }
    SW(static_cast<u16>(W(TBLIST)), 0);   // no objects left in the near table
}

// ---- F3DVEC.ASM: the med/far object table ----

// F3DVEC.ASM: findmem
bool World::findmem(u16& si) {
    si = 0;
    for (int cx = NMAXMEDFAR; cx; cx--) {
        if (segW(0, si + 24u) == 0) {
            SW(EXPTR, static_cast<i16>(si));
            return false;
        }
        si = static_cast<u16>(si + NBYTEXP);
        if (si > MSEPTRMX) si = 0;
    }
    return true;   // no free slot
}

// F3DVEC.ASM: tomemseg
bool World::tomemseg() {
    u16 si;
    if (findmem(si)) return true;
    const u16 src = static_cast<u16>(W(FDEXPOBJ));
    for (int k = 0; k < NBYTEXP; k++) setSegB(0, si + static_cast<u32>(k), B(src + k));
    return false;
}

// F3DVEC.ASM: frommemseg
void World::frommemseg(u16 si, u16 di) {
    for (int k = 0; k < NBYTEXP; k++) SB(di + k, segB(0, si + static_cast<u32>(k)));
}

// STRTEXP.ASM: findexp - a free timed object slot has type 0 and a negative distance code
bool World::findexp(u16& si) {
    si = static_cast<u16>(EXPS);
    for (int cx = NEXPS; cx; cx--) {
        if (W(si + 24u) == 0 && static_cast<int8_t>(B(si + 18u)) < 0) {
            SW(d::strtexp::exptr, static_cast<i16>(si));
            return false;
        }
        si = static_cast<u16>(si + NBYTEXP);
        if (si > d::strtexp::eptrmx) si = static_cast<u16>(EXPS);
    }
    return true;
}

// F3DVEC.ASM: putbackintable - moves memobjbuf into the timed object table, primed
bool World::putbackintable(u16& si) {
    SW(static_cast<u16>(MEMOBJBUF) + 24u, static_cast<i16>(static_cast<u16>(W(static_cast<u16>(MEMOBJBUF) + 24u)) | 0x8000));
    SW(FDEXPOBJ, static_cast<i16>(MEMOBJBUF));
    u16 slot;
    if (findexp(slot)) {
        m_eng.cecode = 63;   // out of space for new timed objects
        return true;
    }
    std::memmove(m_g.raw() + slot, m_g.raw() + MEMOBJBUF, NBYTEXP);
    si = slot;
    return false;
}

// F3DVEC.ASM: drawmemseg - draws the med/far objects; the ones that came close
// enough move into the near table's object array
bool World::drawmemseg() {
    u16 si = 0;
    for (int cx = NMAXMEDFAR; cx; cx--) {
        if (segW(0, si + 24u) != 0) {
            frommemseg(si, static_cast<u16>(MEMOBJBUF));
            if (B(DRAWMEMFLAG) == 0) {
                m_eng.disteval(static_cast<u16>(MEMOBJBUF));   // just evaluate the distance
            } else {
                m_eng.d3dobj(static_cast<u16>(MEMOBJBUF), static_cast<u16>(W(d::m3::ptrgwinf)));
            }
            if (B(MEMOBJBUF + 18) < 2) {   // has this object gotten closer?
                u16 slot;
                putbackintable(slot);
                setSegW(0, si + 24u, 0);   // remove from the memseg table
            }
        }
        si = static_cast<u16>(si + NBYTEXP);
        if (si > MSEPTRMX) si = 0;
        if (m_eng.cecode != 0) break;
    }
    return true;
}

// F3DVEC.ASM: copytable - objects that moved far away leave the near table for memseg
void World::copytable() {
    const u16 tbstrt = static_cast<u16>(W(TBLIST));
    u16 cx = static_cast<u16>(W(tbstrt));
    const u16 bx = static_cast<u16>(tbstrt + 4);   // never advanced: the far items sort to the front
    for (; cx; cx--) {
        const u16 si = static_cast<u16>(W(bx));
        if (B(si + 18u) < 2) continue;
        SW(FDEXPOBJ, static_cast<i16>(si));
        const u16 ext = static_cast<u16>(W(EXTENDEDTYPETBL + 2u * static_cast<u16>(W(si + 72u))));
        bool carry = false;
        if (!(ext & 1)) carry = tomemseg();   // these are not saved
        if (carry) {
            m_eng.cecode = 37;   // memseg is full
            continue;
        }
        SW(si + 24u, 0);   // kill it as a timed object, free up space
        m_eng.tbrmv(tbstrt, si);
    }
}

// ---- STRTEXP.ASM ----

// STRTEXP.ASM: cleanexp - empties the timed object table
void World::cleanexp() {
    // resetnplaced
    SW(NPLACED, 0);
    if (B(ACTIONFLAG) != 0 || B(READFILEFLAG) == 0) SW(NPLACED, 1);
    u16 si = static_cast<u16>(EXPS);
    for (int cx = NEXPS; cx; cx--, si += NBYTEXP) {
        SW(si + 24u, 0);
        SB(si + 18u, 0xFF);
    }
}

// STRTEXP.ASM: expmov, the part that adds primed objects to the near table
void World::expmovPrime() {
    u16 si = static_cast<u16>(EXPS);
    for (int cx = NEXPS; cx; cx--, si += NBYTEXP) {
        if (!(W(si + 24u) & static_cast<i16>(0x8000))) continue;
        SW(si + 24u, static_cast<i16>(W(si + 24u) & 0x7FFF));   // make into a real object
        m_eng.tbadd(static_cast<u16>(W(TBLIST)), si);
    }
}

} // namespace corncob::game
