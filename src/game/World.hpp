// World.hpp - The universe: tiles, the med/far object segment and the theater files
//
// Ports the tile code of F3DVEC.ASM (gettileadr, qintile, encodeobj/decodeobj,
// bringmemseg/farmmemseg, dotiles, flushtiles, drawmemseg, copytable), the file
// readers of 3.ASM (readuniv, readunivfile, readunivparams, getstartfile) and the
// "primed object" half of STRTEXP.ASM expmov. Each method is the ASM proc of the
// same name; procs that pass es:[di] take a segment (a paragraph number inside
// the far segment) and an offset.
//
// The original keeps the universe in one far segment ("memseg"): first the
// med/far object table (nmaxmedfar records of nbytexp bytes, padded to
// memsegskip paragraphs), then the 8x8 tiles of nparatile paragraphs. That is a
// single flat buffer here, addressed as (segment * 16 + offset) relative to
// memseg, so the original's off-by-one reads at the end of the table behave the
// same way.
//
// Theater files (what MOAG.EXE leaves for 3.EXE):
//   3univ.dat   == DEF/<name>.CCT: 64 tiles (row by row) of a 16 byte header
//               ("Pie",0, u16 x, u16 y, u16 count, 6 bytes of 3.EXE's own
//               scratch variables) and count 23 byte objects, then the 12 byte
//               start position (x, y, z as 32-bit pairs).
//   towers.dat  == DEF/<name>.TWR: an optional first line "X sunangle sunsize
//               suncolor gravity rho0 stars rho0z" that readunivparams reads,
//               then the theater name and briefing text.

#pragma once
#include "engine/F3dVec.hpp"
#include "game/GameState.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace corncob::game {

using engine::i16;
using engine::u16;
using engine::u32;
using engine::u8;

class World {
public:
    static constexpr int NBYTEXP = 74;       // timed/object record
    static constexpr int NBYTEOBJ = 23;      // packed tile object
    static constexpr int NOBJTILE = 150;     // objects per tile
    static constexpr int NPARATILE = (NBYTEOBJ * NOBJTILE + 15) / 16;
    static constexpr int NTILESX = 8, NTILESY = 8;
    static constexpr int TILESZ = 32;        // tile edge in high-word units
    static constexpr int NMAXMEDFAR = 884;
    static constexpr int MSEPTRMX = NBYTEXP * NMAXMEDFAR;
    static constexpr int MEMSEGSKIP = (NBYTEXP * NMAXMEDFAR + 15) / 16;
    static constexpr int NHEADERBYTES = 16;
    static constexpr int NDEPTH = 4;
    static constexpr int NEXPS = 320;

    World(GameState& gs, engine::F3dVec& eng);

    // ---- 3.ASM: theater files ----
    // readunivfile: towers.dat (universe parameters), then 3univ.dat (all tiles
    // and the start position). False is the original's "ruerror" exit.
    bool readunivfile(const std::string& towersPath, const std::string& univPath);
    // getstartfile: "x y z yaw" in decimal, replaces the start position
    bool getstartfile(const std::string& path);
    // Parses the 'X ...' parameter line at [kbuf]
    void readunivparams();
    bool readuniv(const std::vector<uint8_t>& file);

    // ---- F3DVEC.ASM: tiles ----
    u16 gettileadr();                   // curtilex/curtiley -> segment (offset 0)
    bool qintile(u16 si);               // carry (true) = object [si] is NOT in the current tile
    void updatect();
    bool qtilefar();                    // carry = too far from ctx,cty
    u16 getctstatusptr();               // address of the tile's "read in" byte
    void farmthem();
    void changethem();
    void bringthem();
    void dotiles();
    void flushtile();
    void flushtiles();
    bool farmmemseg();
    bool bringmemseg();
    bool decodeobj(u16 seg, u16 di, u16 si);        // tile record -> object at [si]; carry = no object
    bool encodeobj(u16 si, u16 seg, u16 di);        // object at [si] -> tile record; carry = bad type
    void gettilesize(u16 seg, u16& di);
    void fillheader();
    bool getheader();

    // ---- F3DVEC.ASM: the med/far object table ----
    bool findmem(u16& si);
    bool tomemseg();                    // object at [fdexpobj] -> a free slot
    void frommemseg(u16 si, u16 di);
    bool drawmemseg();
    void drawmap();                     // draws every object of every tile (map mode)
    bool putbackintable(u16& si);
    void copytable();

    // ---- STRTEXP.ASM ----
    void cleanexp();
    bool findexp(u16& si);
    // expstrt: a free timed object slot filled from the template at tmpl; carry = no room
    bool expstrt(u16 tmpl, u16& si);
    // strtperm: places an object from template tmpl at the 9 words (x, y, z, angles) at src, as a
    // new object of quality initialqual; carry = no room. `slot` is the new object (expobj).
    bool strtperm(u16 src, u16 tmpl, i16 initialqual, u16& slot);
    // strtaa / strtvest / strtdbst: the enemy sites (a dball launcher, a vehicle site, a dball site)
    // placed at the dword x, y, z at src; carry = no room
    bool strtaa(u16 src, i16 initialqual);
    bool strtvest(u16 src, i16 initialqual);
    bool strtdbst(u16 src, i16 initialqual);
    // The first half of expmov: objects primed with bit 15 join the near table.
    // Movement of timed objects (objmov) is Phase 5.
    void expmovPrime();

    // Reads the memseg for tests
    const std::vector<uint8_t>& memseg() const { return m_seg; }

private:
    i16 W(u32 o) const { return m_g.w(o); }
    void SW(u32 o, i16 v) { m_g.setW(o, v); }
    u8 B(u32 o) const { return m_g.b(o); }
    void SB(u32 o, u8 v) { m_g.setB(o, v); }

    // memseg access: seg is a paragraph number relative to memseg
    i16 segW(u32 seg, u32 off) const;
    void setSegW(u32 seg, u32 off, i16 v);
    u8 segB(u32 seg, u32 off) const;
    void setSegB(u32 seg, u32 off, u8 v);

    GameState& m_g;
    engine::F3dVec& m_eng;
    std::vector<uint8_t> m_seg;
};

} // namespace corncob::game
