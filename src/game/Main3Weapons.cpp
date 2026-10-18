// Main3Weapons.cpp - Port of the player's weapons: the guns (3.ASM bulstrt, bulmov), missiles
// (STRTEXP.ASM strtmis, mistim), bombs (strtbomb, strtbomb1, bombtim) and craters (crtrtim),
// and the routine of a bullet that hits something (bdest)
//
// Bullets live in their own table (the 23 records from `bullets` to `bptrmx`), not in the exps
// table; missiles and bombs are timed objects in exps, moved by expmov.

#include "Main3.hpp"
#include "DGroup.hpp"
#include "core/AsmMath.hpp"

namespace corncob::game {

namespace d = corncob::dgroup;
namespace lbl = corncob::lbl;
using asmx::hmul;

#define M3(name) lbl::name

namespace {

inline i16 add16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) + static_cast<u16>(b))); }
inline i16 sub16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) - static_cast<u16>(b))); }
inline i16 neg16(i16 a) { return static_cast<i16>(static_cast<u16>(0u - static_cast<u16>(a))); }
inline i32 add32(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b)); }

constexpr int NBYTEBUL = d::m3_equ::nbytebul;

} // namespace

// getobsvec: the vector from the object at si to the observer (the player, or the plane when he has
// ejected), divided by 64, into dtopv with the sign changed; its length is left in dballdist.
// Returns true (carry) when the observer is too far away.
bool Main3::getobsvec(u16 si, bool playerOnly) {
    const bool far = (B(M3(ejectflag)) != 0 && !playerOnly) ? m_eng.relobj5d(si) : m_eng.rel3d(si);
    if (far) return true;
    const u16 cx = m_eng.rangeBits();
    if (cx >> 8) return true;
    if (cx & 0xF0) return true;   // still too far
    for (int k = 0; k < 3; k++) {
        const i32 dd = D(M3(dxvec) + 4 * k) >> 6;
        SW(M3(dtopv) + 2 * k, neg16(static_cast<i16>(dd & 0xFFFF)));
    }
    SW(M3(dballdist), static_cast<i16>(vmag(static_cast<u16>(M3(dtopv)))));
    return false;
}

// ==========================================================================================
// the guns
// ==========================================================================================

// findbul: the next free bullet; the search wraps at bptrmx. Returns true when none is free.
bool Main3::findbul(u16& si) {
    si = static_cast<u16>(W(M3(bulptr)));
    for (int cx = d::m3_equ::nbuls; cx; cx--) {
        if (W(si + 24u) == 0 && static_cast<int8_t>(B(si + 18u)) < 0) {
            SW(M3(bulptr), static_cast<i16>(si));
            return false;
        }
        si = static_cast<u16>(si + NBYTEBUL);
        if (!(static_cast<i16>(si - static_cast<u16>(M3(bptrmx))) < 0)) si = static_cast<u16>(M3(bullets));
    }
    return true;
}

// bulstrt: fires one bullet from the wings of the plane (or from the man on foot)
void Main3::bulstrt() {
    SW(M3(bulmatptr), static_cast<i16>(M3(nnzmat)));
    SW(M3(bulorgptr), static_cast<i16>(M3(oxeff)));
    if (W(M3(bulyoff)) == 0) SW(M3(bulyoff), static_cast<i16>(d::m3_equ::ebulyoff));
    if (B(M3(ejectflag)) != 0 && B(M3(remoteflag)) == 0) {   // on foot: shots come from the man
        SW(M3(bulmatptr), static_cast<i16>(M3(norot)));
        SW(M3(bulorgptr), static_cast<i16>(M3(ox)));
        SW(M3(bulyoff), 0);
        SW(M3(bulavg), 33);
        SW(M3(ammoleft), sub16(W(M3(ammoleft)), 1));
        if (W(M3(ammoleft)) < 0) {
            SW(M3(ammoleft), 0);
            return;   // out of ammunition
        }
    }
    voiceoff(1, 0x200);   // the bullet sound
    voiceon(1, 0x200);

    u16 bx;
    if (findbul(bx)) return;   // no free bullets
    const u16 mat = static_cast<u16>(W(M3(bulmatptr)));
    const i16 di = add16(W(M3(ctopv)), W(M3(bulvel)));
    SW(bx + 46, hmul(di, W(mat)));
    SW(bx + 48, hmul(di, W(mat + 6)));
    SW(bx + 50, hmul(di, W(mat + 12)));
    SW(bx + 52, uwtadjRef(W(M3(bullife))));   // relative to machine speed
    SB(bx + 18, 0);                         // near
    const u16 org = static_cast<u16>(W(M3(bulorgptr)));
    for (int k = 0; k < 6; k++) SW(bx + 2 * k, W(org + 2 * k));
    // the bullets come from the wings: the offset along the plane's y axis, alternating sides
    const i16 yoff = W(M3(bulyoff));
    SD(bx, add32(D(bx), hmul(yoff, W(mat + 2))));
    SD(bx + 4, add32(D(bx + 4), hmul(yoff, W(mat + 8))));
    SD(bx + 8, add32(D(bx + 8), hmul(yoff, W(mat + 14))));
    SW(M3(bulyoff), neg16(yoff));
    SW(bx + 24, 9);
    if (B(M3(ejectflag)) != 0 && B(M3(remoteflag)) != 0) SW(bx + 24, 1);   // destructable bullets
    m_eng.tbadd(static_cast<u16>(W(d::f3dvec::tblist)), bx);
    SW(M3(bulavg), add16(W(M3(bulavg)), 2));
}

// bobjmov: a bullet flies; when it hits the ground it makes a puff of smoke and stops
void Main3::bobjmov(u16 si) {
    const u16 disp = UW(M3(objdisp));
    auto axisVel = [&](u16 velOff) -> i16 {
        i16 ax = sub16(static_cast<i16>(m_rand.randr_w(disp)), static_cast<i16>(disp >> 1));
        ax = add16(ax, W(velOff));
        SW(velOff, ax);
        return ax;
    };
    SD(si, add32(D(si), dwtadj(axisVel(si + 46))));
    SD(si + 4, add32(D(si + 4), dwtadj(axisVel(si + 48))));
    const i16 vz = axisVel(si + 50);
    const i32 z = add32(D(si + 8), dwtadj(sub16(vz, W(M3(tckgrav)))));
    SD(si + 8, z);
    if (z >= 0) return;
    SD(si + 8, 0);
    strtpuff(si);
    for (int k = 0; k < 4; k++) SW(si + 46 + 2 * k, 0);   // the velocity and the time left
}

// bulmov: moves the bullets, and retires the ones that ran out of time. (The original loops 32 times
// over a table of 23 records and so walks into the templates that follow it; only the table is used.)
void Main3::bulmov() {
    SW(M3(objdisp), W(M3(buldisp)));   // dispersion of the bullet velocities
    SW(M3(tckgrav), 54);
    for (u16 si = static_cast<u16>(M3(bullets)); si < static_cast<u16>(M3(bptrmx)); si = static_cast<u16>(si + NBYTEBUL)) {
        if (W(si + 52) <= 0) {
            SW(si + 24, 0);   // bulkil
            continue;
        }
        SW(si + 52, sub16(W(si + 52), static_cast<i16>(m_refSteps)));
        bobjmov(si);
    }
}

// bdest: a bullet that hit something is a little spray of sparks, harmless to what it hit
void Main3::bdest(u16 si, u16 di, bool carry) {
    if (!carry) return;
    SB(M3(flttype), static_cast<u8>(B(static_cast<u16>(di + 19)) & 3));   // make flotsam harmless
    SB(M3(omniflag), 1);   // omni-directional
    strtflt(si, 2);
    SB(M3(flttype), 0);
    SW(si + 24, 0);   // remove the bullet from the table
}

// ==========================================================================================
// missiles
// ==========================================================================================

// strtmis: launches a missile; returns true when there was no room for one
bool Main3::strtmis() {
    u16 bx;
    SW(M3(expobj), static_cast<i16>(M3(fltsm1mis)));
    if (m_world.expstrt(static_cast<u16>(M3(fltsm1mis)), bx)) return true;
    SW(M3(expobj), static_cast<i16>(bx));
    SB(bx + 18, 0);
    SW(bx + 52, static_cast<i16>(d::m3_equ::mistime));
    SW(bx + 24, static_cast<i16>(UW(bx + 24) | 0x8000));   // a timed object
    const u16 mat = static_cast<u16>(M3(nnzmat));
    const i16 di = add16(W(M3(ctopv)), 125);
    SW(bx + 46, hmul(di, W(mat)));
    SW(bx + 48, hmul(di, W(mat + 6)));
    SW(bx + 50, hmul(di, W(mat + 12)));
    SB(bx + 18, 0);
    for (int k = 0; k < 6; k++) SW(bx + 2 * k, W(M3(oxeff) + 2 * k));
    for (int k = 0; k < 3; k++) SW(bx + 12 + 2 * k, W(M3(orveceff) + 2 * k));   // the angles of the plane too
    const i16 yoff = W(M3(misyoff));   // missiles come from the wings
    SD(bx, add32(D(bx), hmul(yoff, W(mat + 2))));
    SD(bx + 4, add32(D(bx + 4), hmul(yoff, W(mat + 8))));
    SD(bx + 8, add32(D(bx + 8), hmul(yoff, W(mat + 14))));
    SW(M3(misyoff), neg16(yoff));
    return false;
}

// mistim: a missile; carry = it was hit or ran out
void Main3::mistim(u16 si, bool carry) {
    auto toHell = [&]() {
        SB(M3(boomdeadly), 0x20);   // make it deadly
        strtboom(si);
        SB(M3(omniflag), 1);        // an all-around explosion
        strtflt(si, 3);
        SW(si + 24, 0);             // cancel the object
    };
    if (carry) {
        toHell();
        return;
    }
    if ((W(si + 10) | W(si + 8)) == 0) {   // in the ground: explode
        toHell();
        return;
    }
    if (W(si + 52) < d::m3_equ::mistime - 11) return;   // the burn is over (after about a second)
    if (vmag(static_cast<u16>(si + 46)) > 3000) return;   // not faster than 3000
    for (int k = 0; k < 3; k++) {   // speed up by 1000/890
        const i32 v = static_cast<i32>(W(si + 46 + 2 * k)) * 1000 / 890;
        SW(si + 46 + 2 * k, static_cast<i16>(v & 0xFFFF));
    }
}

// ==========================================================================================
// bombs and craters
// ==========================================================================================

// the sub / sbb pair the original uses to take a 16-bit amount off a dword whose high word is not
// involved except for the borrow
static i16 subBorrow(GameState& g, u32 lo, u32 hi, i16 amount) {
    const u16 old = g.uw(lo);
    g.setW(lo, static_cast<i16>(old - static_cast<u16>(amount)));
    if (old < static_cast<u16>(amount)) g.setW(hi, static_cast<i16>(g.uw(hi) - 1));
    return amount;
}

// strtbomb: drops a bomb below and behind the plane, carrying a part of its velocity
bool Main3::strtbomb() {
    u16 bx;
    SW(M3(expobj), static_cast<i16>(M3(fltsm1bomb)));
    if (m_world.expstrt(static_cast<u16>(M3(fltsm1bomb)), bx)) return true;
    SW(M3(expobj), static_cast<i16>(bx));
    SB(bx + 18, 0);
    SW(bx + 52, static_cast<i16>(d::m3_equ::bombtime));
    SW(bx + 24, static_cast<i16>(UW(bx + 24) | 0x8000));      // a timed object
    SW(bx + 24, static_cast<i16>(UW(bx + 24) & ~0xE8));       // strip the destructable bit
    for (int k = 0; k < 6; k++) SW(bx + 2 * k, W(M3(oxeff) + 2 * k));
    for (int k = 0; k < 3; k++) SW(bx + 12 + 2 * k, W(M3(orveceff) + 2 * k));   // the angle of the plane too
    // the bomb takes a sixteenth of the plane's velocity (the original works on the low words)
    const u32 rt = M3(rtopv);
    i16 ax = static_cast<i16>(W(rt) >> 4);
    subBorrow(m_g, rt, rt + 2, ax);
    SW(bx + 46, ax);
    ax = static_cast<i16>(W(rt + 2) >> 4);
    SW(bx + 48, ax);
    subBorrow(m_g, rt + 4, rt + 6, ax);
    ax = static_cast<i16>(W(rt + 4) >> 4);
    SW(bx + 50, ax);
    subBorrow(m_g, rt + 8, rt + 10, ax);
    // start the bomb below and behind the plane
    SW(M3(xvec), -500);
    SW(M3(xvec) + 2, 0);
    SW(M3(xvec) + 4, -500);
    m_eng.matvmul(static_cast<u16>(M3(xvec)), static_cast<u16>(M3(nnzmat)), static_cast<u16>(M3(xvec1)));
    for (int k = 0; k < 3; k++) SD(bx + 4 * k, add32(D(bx + 4 * k), static_cast<i32>(W(M3(xvec1) + 2 * k))));
    return false;
}

// strtbomb1: a bomb at the position and with the velocity of the object at si (a saucer, or the man)
bool Main3::strtbomb1(u16 src) {
    u16 bx;
    SW(M3(expobj), static_cast<i16>(M3(fltsm1bomb)));
    if (m_world.expstrt(static_cast<u16>(M3(fltsm1bomb)), bx)) return true;
    SW(M3(expobj), static_cast<i16>(bx));
    SB(bx + 18, B(src + 18));
    SB(bx + 19, 2);   // a class II bomb
    SW(bx + 52, static_cast<i16>(d::m3_equ::bombtime));
    SW(bx + 24, static_cast<i16>(UW(bx + 24) | 0x8000));
    for (int k = 0; k < 6; k++) SW(bx + 2 * k, W(src + 2 * k));
    for (int k = 0; k < 3; k++) SW(bx + 46 + 2 * k, W(src + 46 + 2 * k));   // the velocity
    return false;
}

// bombtim: the bomb falls, arms itself once away from the observer, and blows up with a crater
void Main3::bombtim(u16 si, bool carry) {
    auto toHell = [&]() {
        SB(M3(boomdeadly), 0x60);   // a six pack
        strtboom(si);
        const u16 pieces = (W(si + 24) & 2) ? 5 : 15;   // a saucer's bomb is less spectacular
        strtincend(si, pieces);
        bigboom();
        if (W(si + 10) != 0 || UW(si + 8) > 10000) {   // exploded too high in the air: just kill the bomb
            SW(si + 24, 0);
            return;
        }
        const i16 crater = static_cast<i16>(M3(crtrobj));
        SW(si + 40, crater);
        SW(si + 42, crater);
        SW(si + 44, crater);
        SW(si + 72, 32);   // make it into a crater
        SW(si + 8, 10);    // slightly above the ground
        SW(si + 10, 0);
        SB(si + 18, 3);    // far away, to avoid a crash on the first frame after dropping
        SW(si + 12, 0);
        SW(si + 14, 0);
        SW(si + 16, 0);
        SB(si + 26, 0);    // no rotation
        u16 bx = static_cast<u16>(uwtadjRef(static_cast<i16>(W(M3(cratertime)))));   // a crater lasts only so long
        SW(M3(cratertimem1), static_cast<i16>(bx - 100));
        if (W(si + 24) & 2) bx >>= 4;
        SW(si + 52, static_cast<i16>(bx));
        SW(si + 24, static_cast<i16>(UW(si + 24) & 0x911));   // harmless to the plane
    };
    if (carry) {
        toHell();
        return;
    }
    SB(M3(bombclr), (W(si + 24) & 0x800) ? 15 : 7);   // an assassin bomb is white
    if (!(W(si + 24) & 4)) {   // not planted
        if (W(si + 10) < 0) { toHell(); return; }   // blow up if under the surface
        if ((W(si + 10) | W(si + 8)) == 0) { toHell(); return; }
        if (!getobsvec(si) && UW(M3(dballdist)) >= 200)
            SW(si + 24, static_cast<i16>(UW(si + 24) | 0xE8));   // arm the bomb, and make collisions possible
    }
    if (W(si + 24) & 4) {   // planted: it waits for the detonate command
        if (!key(0x20)) return;   // 'd'
        SW(si + 24, static_cast<i16>(UW(si + 24) | 0xE8));
        SW(si + 24, static_cast<i16>(UW(si + 24) & ~4));
        SW(si + 66, static_cast<i16>(d::m3_equ::bomblst));
    }
    // bombinair: air resistance, -k v squared
    const i16 bx = neg16(static_cast<i16>(vmag(static_cast<u16>(si + 46))));
    constexpr int K = 4630;
    for (int k = 0; k < 3; k++) {
        i32 ax = static_cast<i32>(static_cast<i16>(W(si + 46 + 2 * k))) * bx / K;
        if (k == 2) ax -= 54;   // gravity
        SW(si + 46 + 2 * k, add16(W(si + 46 + 2 * k), static_cast<i16>(ax & 0xFFFF)));
    }
}

// crtrtim: a crater is harmless to start with and only turns lethal when it has settled
void Main3::crtrtim(u16 si, bool carry) {
    if (carry) return;
    SB(M3(crtrclr), 0);   // black by default
    if (W(si + 24) & 0x800) {   // lives forever
        SW(si + 52, add16(W(si + 52), static_cast<i16>(m_refSteps)));
        SW(si + 24, static_cast<i16>(UW(si + 24) | 0xE0));   // lethal for the plane
        SB(M3(crtrclr), 6);   // brown
        return;
    }
    if (UW(M3(cratertimem1)) < UW(si + 52)) return;
    SW(si + 24, static_cast<i16>(UW(si + 24) | 0xE0));
}

} // namespace corncob::game
