// F3dObj.cpp - F3DVEC.ASM object interaction: collisions, walking, plane crashes, timed objects
//
// The distance vector dxvec of the object being looked at is left in DGROUP by the drawing
// code (rel3d inside disteval), exactly as the ASM leaves it for these procs.
//
// "js/jns" after CMP/SUB tests the sign of the 16-bit difference (sf16); jb/jae/ja/jbe are
// unsigned. A proc's carry result is the bool it returns (true = carry set).

#include "F3dVec.hpp"
#include "AsmTables.hpp"
#include "DGroup.hpp"
#include "core/AsmMath.hpp"
#include <cstdlib>

namespace corncob::engine {

namespace d = corncob::dgroup;
using asmx::sf16;

namespace {

constexpr u32 DXVEC = d::m3::dxvec, OX = d::m3::ox;
constexpr u32 OBJ5D = d::defobjs::obj5d;
constexpr u32 EXTENDEDTYPETBL = d::defobjs::extendedtypetbl;
constexpr u32 OBJTYPE = d::f3dvec::objtype, EXTENDEDTYPE = d::f3dvec::extendedtype;
constexpr u32 X1VEC = d::f3dvec::x1vec, X2VEC = d::f3dvec::x2vec;
constexpr u32 X1MAG = d::f3dvec::x1mag, X2MAG = d::f3dvec::x2mag;
constexpr u32 XDOT = d::f3dvec::xdot, XDOTS = d::f3dvec::xdots;
constexpr u32 WALLDAMTICK = d::f3dvec::walldamtick, BUILDING = d::f3dvec::building;
constexpr u32 ARDVARK = d::f3dvec::ardvark, TEMP1 = d::f3dvec::temp1;
constexpr u32 EJECTFLAG = d::m3::ejectflag, REMOTEFLAG = d::m3::remoteflag;
constexpr u32 INVINCIBLE = d::m3::invincible, CRSHFLG = d::m3::crshflg;
constexpr u32 MISSIONSTATUS = d::m3::missionstatus, DEATHCODE = d::m3::deathcode;
constexpr u32 DEATHCOORDS = d::f3dvec::deathcoords, CAPTUREDFLAG = d::m3::capturedflag;
constexpr u32 OZ = d::m3::oz, MYZGROUND = d::m3::myzground, ZGROUND = d::m3::zground;
constexpr u32 ONRUNWAYFLAG = d::m3::onrunwayflag;
constexpr u32 BRDRCNT = d::m3::brdrcnt, BRDRCLR = d::f3dvec::brdrclr, TOTALDAMAGE = d::m3::totaldamage;
constexpr u32 RTOPV = d::m3::rtopv;
constexpr int EHMIN = 720;   // F3DEQU.INC ehmin = 4*3*l

inline u32 abs32(i32 v) { return v < 0 ? 0u - static_cast<u32>(v) : static_cast<u32>(v); }

} // namespace

// F3DVEC.ASM: vmag - takes a vector at [si], returns its magnitude (unsigned)
u16 F3dVec::vmag(u16 si) {
    u32 sum = 0;
    for (int k = 0; k < 3; k++) {
        const i32 a = W(si + 2 * k);
        sum += static_cast<u32>(a * a);
    }
    return dsqrt(sum);
}

// F3DVEC.ASM: relobj5d - like rel3d, with the plane (obj5d) as the origin
bool F3dVec::relobj5d(u16 si) {
    m_range = 0;
    for (int k = 0; k < 3; k++) {
        const i32 dd = static_cast<i32>(static_cast<u32>(m_g.dw(si + 4 * k)) - static_cast<u32>(m_g.dw(OBJ5D + 4 * k)));
        m_g.setDw(DXVEC + 4 * k, dd);
        m_range |= static_cast<u16>(abs32(dd) >> 15);
    }
    return false;
}

void F3dVec::callRoutine(u16 si, u16 di, bool carry) {
    if (m_hooks) m_hooks->objroutine(static_cast<u16>(W(si + 72)), si, di, carry);
}

// F3DVEC.ASM: qtimed
void F3dVec::qtimed(u16 si) {
    if (!(W(si + 24) & 16)) return;   // is this a timed object?
    callRoutine(si, 0, false);
}

// F3DVEC.ASM: slam - two objects collided: let each one that can blow up do so
void F3dVec::slam(u16 si, u16 di) {
    if (W(si + 24) & 8) callRoutine(si, di, true);
    if (W(di + 24) & 8) callRoutine(di, si, true);
}

// F3DVEC.ASM: divs_qd (sas.lib) - the quad at [di] divided by a signed dword. The collision code
// puts a dot product in the upper dword, so the quotient is a 32.32 fixed point number.
void F3dVec::divs_qd(u16 di, i32 den) {
    int64_t n = 0;
    for (int k = 3; k >= 0; k--) n = (n << 16) | static_cast<uint16_t>(W(di + 2 * k));
    int64_t q = den == 0 ? (n >= 0 ? INT64_MAX : INT64_MIN) : n / den;
    for (int k = 0; k < 4; k++) {
        SW(di + 2 * k, static_cast<i16>(static_cast<uint64_t>(q) & 0xFFFF));
        q >>= 16;
    }
}

// F3DVEC.ASM: getx1 - vector between missile [si] and target [di] into x1vec / x1mag;
// carry when too far off
bool F3dVec::getx1(u16 si, u16 di) {
    for (int k = 0; k < 3; k++) {
        const i32 dd = static_cast<i32>(static_cast<u32>(m_g.dw(si + 4 * k)) - static_cast<u32>(m_g.dw(di + 4 * k)));
        SW(X1VEC + 2 * k, static_cast<i16>(dd));
        const u32 a = abs32(dd);
        if ((a >> 16) != 0) return true;   // high word must be zero
        if (a & 0x8000) return true;       // and the high bit low
    }
    SW(X1MAG, static_cast<i16>(vmag(static_cast<u16>(X1VEC))));
    return false;
}

// F3DVEC.ASM: oldqcollis - box test by xyz distance, not by radius. Carry = no collision.
void F3dVec::oldqcollis(u16 si, u16 di, bool& carry) {
    carry = true;
    u32 sum = static_cast<u16>(W(di + 66));
    if (sum == 0) return;
    if (W(si + 66) == 0) return;
    for (int k = 0; k < 3; k++) {
        const u16 off = static_cast<u16>(66 + 2 * k);
        sum = static_cast<u32>(static_cast<u16>(W(di + off))) + static_cast<u16>(W(si + off));
        const u32 a = abs32(static_cast<i32>(static_cast<u32>(m_g.dw(si + 4 * k)) - static_cast<u32>(m_g.dw(di + 4 * k))));
        if (a >= sum) return;   // jae movon
    }
    slam(si, di);
    carry = false;
}

// F3DVEC.ASM: qcollis1 - normal object against the next one in the table
void F3dVec::qcollis1(u16 si, u16 di) {
    const u16 flags = static_cast<u16>(W(si + 24) | W(di + 24));
    if (!(flags & 8)) return;   // at least one of them must be able to blow
    if (flags & 0x100) {        // walls: the box test
        bool c;
        oldqcollis(si, di, c);
        return;
    }
    u16 bx = static_cast<u16>(W(di + 66));
    if (bx == 0) return;
    const u16 ax = static_cast<u16>(W(si + 66));
    if (ax == 0) return;
    if (bx < ax) bx = static_cast<u16>(bx + W(si + 66));   // criterion is the sum when the first is smaller
    if (getx1(si, di)) return;
    if (static_cast<u16>(W(X1MAG)) >= bx) return;
    slam(si, di);
}

// F3DVEC.ASM: qvelon - is the missile at [si] heading for the object at [di]? Carry = no.
bool F3dVec::qvelon(u16 si, u16 di) {
    if (!(W(di + 24) & 8)) return true;
    if (getx1(si, di)) return true;
    u16 bx = static_cast<u16>(W(di + 66));
    if (bx < static_cast<u16>(W(si + 66))) bx = static_cast<u16>(W(si + 66));
    if (bx >= static_cast<u16>(W(X1MAG))) {   // already a collision
        slam(si, di);
        return false;
    }
    const u16 x2mag = vmag(static_cast<u16>(si + 46));   // magnitude of the velocity
    SW(X2MAG, static_cast<i16>(x2mag));
    for (int k = 0; k < 3; k++) SW(X2VEC + 2 * k, W(si + 46 + 2 * k));
    // dot product x1 . velocity into the upper dword of xdot
    i32 dot = 0;
    for (int k = 0; k < 3; k++) dot = static_cast<i32>(static_cast<u32>(dot) + static_cast<u32>(static_cast<i32>(W(X1VEC + 2 * k)) * W(si + 46 + 2 * k)));
    m_g.setDw(XDOT + 4, dot);
    m_g.setDw(XDOTS, dot);
    const u32 den = static_cast<u32>(static_cast<u16>(W(X1MAG))) * x2mag;
    divs_qd(static_cast<u16>(XDOT), static_cast<i32>(den));
    if (!(W(XDOT + 6) & static_cast<i16>(0x8000))) return true;   // moving away: failed to line up
    const u32 frac = static_cast<u32>(m_g.dw(XDOT));
    const u32 negfrac = 0u - frac;                                    // neg_d dx,ax
    const u16 dx = static_cast<u16>(negfrac >> 16);
    bool albang4 = true;
    if (W(XDOT + 4) != -1) albang4 = true;                            // |cos| > 1
    else albang4 = dx >= 45000;                                       // cos < -0.687
    if (!albang4) return true;                                        // linupfl
    if (static_cast<u16>(W(X1MAG)) / 2 <= static_cast<u16>(W(X2MAG))) {   // albang5
        slam(si, di);
        return false;
    }
    return false;
}

// F3DVEC.ASM: qcollis - looks between object si (at table slot bx) and the others for a collision
void F3dVec::qcollis(u16 si, u16 bx) {
    const u16 type = static_cast<u16>(W(si + 72));
    if (type == 21 || type == 30) {   // a missile or a bullet moves fast: look at everything nearer
        if (W(si + 24) == 0) return;
        if (W(si + 52) == 0) return;
        for (;;) {
            bx = static_cast<u16>(bx - 2);
            if (sf16(static_cast<i16>(bx), static_cast<i16>(m_tbstrt + 4))) return;   // before the table
            const u16 di = static_cast<u16>(W(bx));
            if (B(si + 19) == B(di + 19)) continue;   // like objects don't collide
            if (W(di + 24) == 0) continue;
            bool carry;
            if ((W(si + 24) | W(di + 24)) & 0x100) oldqcollis(si, di, carry);   // a wall segment
            else carry = qvelon(si, di);
            if (carry) continue;
            return;
        }
    }
    const u16 di = static_cast<u16>(W(bx + 2));
    if (B(si + 19) == B(di + 19)) return;   // like objects can't collide
    qcollis1(si, di);
}

// F3DVEC.ASM: qwalk - sees if the person has collided with an object
void F3dVec::qwalk(u16 si) {
    SW(OBJTYPE, W(si + 24));
    const u16 ext = static_cast<u16>(W(EXTENDEDTYPETBL + 2u * static_cast<u16>(W(si + 72))));
    SW(EXTENDEDTYPE, static_cast<i16>(ext));
    if (m_rdf != 0) return;           // must be right close
    rel3d(si);
    if (m_range >> 8) return;         // make sure we have the current coords
    if (!(ext & (64 + 128 + 512))) return;   // walkable, killable or capturing
    for (int k = 0; k < 3; k++) {
        const u32 a = abs32(m_g.dw(DXVEC + 4 * k));
        if ((a >> 16) != 0) return;
        if (static_cast<u16>(a) >= static_cast<u16>(W(si + 66 + 2 * k))) return;
    }
    if (ext & 128) {   // does it kill us?
        if (B(INVINCIBLE)) return;
        SW(DEATHCODE, W(si + 72));
        SB(CRSHFLG, 10);   // we died from an object
        SW(MISSIONSTATUS, static_cast<i16>(static_cast<u16>(W(MISSIONSTATUS)) | 0x800));
        for (int k = 0; k < 6; k++) SW(DEATHCOORDS + 2 * k, W(DXVEC + 2 * k));   // fuckyou
        return;
    }
    if (ext & 512) {   // does it capture us?
        if (B(INVINCIBLE)) return;
        SW(DEATHCODE, W(si + 72));
        SB(CAPTUREDFLAG, 0xFF);
        return;
    }
    // we have collided with something to stand on
    u32 height = static_cast<u32>(m_g.dw(si + 8));
    if (ext & 256) height = height + static_cast<u16>(W(si + 70)) - (EHMIN + 1);   // include its extent
    const i32 diff = static_cast<i32>(static_cast<u32>(m_g.dw(OZ)) + EHMIN - height);
    if (diff >= 0) m_g.setDw(MYZGROUND, static_cast<i32>(height));
}

// F3DVEC.ASM: qcrash - sees if the plane has collided with an object
void F3dVec::qcrash(u16 si) {
    SW(OBJTYPE, W(si + 24));
    const u16 type = static_cast<u16>(W(si + 72));
    if (type == 28) return;   // don't collide with object 5d
    const u16 ext = static_cast<u16>(W(EXTENDEDTYPETBL + 2u * type));
    SW(EXTENDEDTYPE, static_cast<i16>(ext));

    if (W(si + 66) < 0) {   // an object with a negative collision radius: registration violation
        u8 al = 97;
        if (static_cast<u8>(W(si + 66)) == 0xFE) al++;
        cecode = al;
        if (type != 31) cecode = 59;
        SW(TEMP1, static_cast<i16>(type));
        SB(EJECTFLAG, 0xFF);   // just pop you out of the plane
    }

    if (B(EJECTFLAG)) {
        if (W(OBJ5D + 24) == 0) return;   // is the plane around?
        relobj5d(si);
        if (m_range >> 8) return;
        if (m_range & 0xF0) return;       // still too far
    } else if (m_rdf != 0) {
        return;                           // must be right close
    }
    const u16 objtype = static_cast<u16>(W(OBJTYPE));
    if (!(objtype & 0x20) && !(ext & 16)) return;   // crashable, or a runway piece
    for (int k = 0; k < 3; k++) {
        const u32 a = abs32(m_g.dw(DXVEC + 4 * k));
        if ((a >> 16) != 0) return;
        if (static_cast<u16>(a) >= static_cast<u16>(W(si + 66 + 2 * k))) return;
    }
    if (ext & 16) {   // a runway piece
        SB(ONRUNWAYFLAG, 1);   // all are friendly
        m_g.setDw(ZGROUND, m_g.dw(si + 8));
        return;
    }
    if ((ext & 0x20) || (objtype & 8)) {   // a portal, or destructible
        if (ext & 4) {
            callRoutine(si, 0xFFFF, true);   // custom destruction, di = -1 means "the plane"
        } else {
            SW(si + 52, 0);                  // cancel the object
        }
    }
    u16 cx = 1;   // default is 1 damage unit to the plane
    bool skipRest = false;
    if (objtype & 0x40) {   // a six pack of damage
        cx = 6;
        SW(BRDRCNT, 8);
        if (B(BRDRCLR) == 4) {
            skipRest = true;   // already terminally damaged: straight to the wall test
        } else {
            SB(BRDRCLR, 7);
        }
    }
    if (!skipRest && (objtype & 0x80)) {   // instant destruction
        cx = 75;
        if (static_cast<u16>(W(TOTALDAMAGE)) > cx) return;
        SB(BRDRCLR, 4);   // give warning of destruction
        SW(BRDRCNT, 0);
        for (int k = 0; k < 2; k++) {   // bounce the plane off the object
            const i16 v = W(RTOPV + 2 * k);
            SW(RTOPV + 2 * k, static_cast<i16>(static_cast<i16>(-v) >> 1));
        }
        SW(MISSIONSTATUS, static_cast<i16>(static_cast<u16>(W(MISSIONSTATUS)) | 0x80));
        SW(BUILDING, W(si + 72));
    }
    if (ext & 8) {   // a wall: damage only every 200 ticks, and double
        const u16 now = m_clockTicks ? *m_clockTicks : 0;
        if (static_cast<u16>(now - static_cast<u16>(W(WALLDAMTICK))) < 200) return;
        SW(WALLDAMTICK, static_cast<i16>(now));
        cx = static_cast<u16>(cx + cx);
    }
    const bool boom = B(REMOTEFLAG) == 0 && B(EJECTFLAG) != 0;   // makeaboom
    if (boom) SW(ARDVARK, W(si + 72));
    if (m_hooks) {
        m_hooks->crashSound(boom);
        m_hooks->damageplane(cx);
    }
}

// ---- ground detail points (3.ASM's gscum segment, drawn by F3DVEC.ASM g3dobj) ----

i32 F3dVec::gsDw(u32 off) const {
    if (off + 4 > m_gscum.size()) return 0;
    return static_cast<i32>(static_cast<u32>(m_gscum[off]) | (static_cast<u32>(m_gscum[off + 1]) << 8) |
                            (static_cast<u32>(m_gscum[off + 2]) << 16) | (static_cast<u32>(m_gscum[off + 3]) << 24));
}

// F3DVEC.ASM: grel3d - the point's coordinates relative to the observer, into dxvec
void F3dVec::grel3d(u32 point) {
    m_range = 0;
    for (int k = 0; k < 3; k++) {
        const i32 dd = static_cast<i32>(static_cast<u32>(gsDw(point + 4 * k)) - static_cast<u32>(m_g.dw(OX + 4 * k)));
        m_g.setDw(DXVEC + 4 * k, dd);
        m_range |= static_cast<u16>(abs32(dd) >> 15);
    }
}

// F3DVEC.ASM: srel3d - the same for a star, with the distance divided by 256
void F3dVec::srel3d(u32 point) {
    m_range = 0;
    for (int k = 0; k < 3; k++) {
        const i32 dd = static_cast<i32>(static_cast<u32>(gsDw(point + 4 * k)) - static_cast<u32>(m_g.dw(OX + 4 * k)));
        const i32 sh = dd >> 8;
        m_g.setDw(DXVEC + 4 * k, sh);
        m_range |= static_cast<u16>(abs32(sh) >> 15);
    }
}

// F3DVEC.ASM: g3dobj - evaluates the distance of a dot and plots it; carry = too far (or
// off the screen), so the caller makes a new one
bool F3dVec::g3dobj(u32 point, bool star) {
    SW(d::f3dvec::xtemp, static_cast<i16>(gsDw(point + 12) & 0xFFFF));
    if (star) srel3d(point);
    else grel3d(point);
    if (m_range >> 8) return true;
    const u8 cl = static_cast<u8>(m_range & 0xFF);
    constexpr u32 XVEC = d::m3::xvec;
    if (cl & 0xF0) {            // medium far
        m_rdf = 8;
        for (int k = 0; k < 3; k++) {
            const u32 dd = static_cast<u32>(m_g.dw(DXVEC + 4 * k));
            SW(XVEC + 2 * k, static_cast<i16>(static_cast<i16>(static_cast<u16>(dd >> 8)) >> 1));
        }
    } else if (cl & 0x0F) {     // medium
        m_rdf = 4;
        for (int k = 0; k < 3; k++) {
            const u32 dd = static_cast<u32>(m_g.dw(DXVEC + 4 * k));
            SW(XVEC + 2 * k, static_cast<i16>(static_cast<i16>(static_cast<u16>(dd >> 4)) >> 1));
        }
    } else {                    // close
        for (int k = 0; k < 3; k++) SW(XVEC + 2 * k, static_cast<i16>(W(DXVEC + 4 * k) >> 1));
    }
    m_objptr = static_cast<u16>(d::f3dvec::tmpvec);
    gnrlrot(static_cast<u16>(XVEC));
    SB(d::m3::n, B(d::m3::gndcolor));
    const u16 si = m_objptr;
    const u16 bx = static_cast<u16>(W(d::m3::ptrgwinf));
    if (W(d::f3dvec::xtemp) == 0) return f3dpt(si, bx);
    viewpt(si);
    return false;
}

} // namespace corncob::engine
