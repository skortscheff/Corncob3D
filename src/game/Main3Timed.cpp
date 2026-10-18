// Main3Timed.cpp - Port of STRTEXP.ASM's timed object engine and the explosions: objmov, expmov,
// flotsam (strtflt, flttim), shards, incendiaries, puffs and booms, and the destroy paths of
// DEFOBJS.ASM that use them (fdest, gmdest, odest's explosion)
//
// A timed object is a 74-byte record in the exps table: [24] type bits (8000h = primed, joins the
// near table next pass), [52] time left, [46..50] velocity, [54..58] spin, [60] dispersion,
// [62] damping, [64] gravity, [65] a boom when it times out. Every proc takes the record as a
// DGROUP offset, as the ASM took it in si.

#include "Main3.hpp"
#include "DGroup.hpp"
#include "core/AsmMath.hpp"

namespace corncob::game {

namespace d = corncob::dgroup;
namespace lbl = corncob::lbl;

#define M3(name) lbl::name

namespace {

inline i16 add16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) + static_cast<u16>(b))); }
inline i16 sub16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) - static_cast<u16>(b))); }
inline i16 neg16(i16 a) { return static_cast<i16>(static_cast<u16>(0u - static_cast<u16>(a))); }
inline i32 add32(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b)); }

constexpr u32 EXPS = d::strtexp::exps;
constexpr int NBYTEXP = d::m3_equ::nbytexp, NEXPS = d::m3_equ::nexps;

} // namespace

// ==========================================================================================
// objmov: moves the timed object at si by its velocity, dispersion, damping and gravity.
// Returns true (carry) when it hit the ground, which also stops it dead.
// ==========================================================================================
bool Main3::objmov(u16 si) {
    if (B(si + 26) != 0) {   // apply rotation
        SW(si + 12, add16(W(si + 12), static_cast<i16>(frameScale(W(si + 54)))));
        SW(si + 14, add16(W(si + 14), static_cast<i16>(frameScale(W(si + 56)))));
        SW(si + 16, add16(W(si + 16), static_cast<i16>(frameScale(W(si + 58)))));
    }
    // the random push is a walk, so it grows with the square root of the number of frames
    const u16 disp = dispScale(UW(M3(objdisp)));
    const u16 damp = UW(M3(objdamp));

    // one axis: the velocity at velOff gets a random push, then air resistance against its sign
    auto axisVel = [&](u16 velOff) -> i16 {
        i16 ax = 0;
        if (disp != 0) {
            ax = static_cast<i16>(m_rand.randr_w(disp));
            const u16 half = static_cast<u16>((disp >> 1) + (disp & 1));   // round evenly
            ax = sub16(ax, static_cast<i16>(half));
        }
        if (damp != 0) {
            ax = add16(ax, W(velOff));
            SW(velOff, ax);
            const bool negative = ax < 0;   // the flags of that add decide the sign of the drag
            const u32 square = static_cast<u32>(static_cast<i32>(ax) * static_cast<i32>(ax));
            ax = static_cast<i16>(frameScale(static_cast<i32>((square / damp) & 0xFFFF)));   // the drag of this frame
            if (!negative) ax = neg16(ax);
        }
        ax = add16(ax, W(velOff));
        SW(velOff, ax);
        return ax;
    };

    i16 ax = axisVel(si + 46);
    SD(si, add32(D(si), dwtadj(ax)));
    ax = axisVel(si + 48);
    SD(si + 4, add32(D(si + 4), dwtadj(ax)));
    ax = axisVel(si + 50);

    // gravity; a value of 55 means none (and the velocity is left alone)
    const bool noGravity = UW(M3(tckgrav)) == 55;
    const i16 pull = noGravity ? W(M3(tckgrav)) : wtadj(W(M3(tckgrav)));
    ax = sub16(ax, pull);
    if (!noGravity) SW(si + 50, ax);
    const i32 z = add32(D(si + 8), dwtadj(ax));
    SD(si + 8, z);
    if (z >= 0) return false;
    if (W(si + 50) == 0) return false;   // (not moving: leave it)

    // under the ground: stop it, and its spin if it has one
    SD(si + 8, 0);
    SW(si + 46, 0);
    SW(si + 48, 0);
    SW(si + 50, 0);
    if (B(si + 26) != 0) {
        for (int k = 0; k < 6; k++) SW(si + 54 + 2 * k, 0);   // spin, dispersion, damping, gravity
    }
    return true;
}

// ==========================================================================================
// expmov: primes new timed objects (they join the near table), then moves each live one a tick;
// the ones that run out of time vanish, with a boom if they carry one
// ==========================================================================================
void Main3::expmov() {
    u16 si = static_cast<u16>(EXPS);
    for (int cx = NEXPS; cx; cx--, si = static_cast<u16>(si + NBYTEXP)) {
        if (UW(si + 24) & 0x8000) {   // primed for creation: make it a real object
            SW(si + 24, static_cast<i16>(UW(si + 24) & 0x7FFF));
            m_eng.tbadd(static_cast<u16>(W(d::f3dvec::tblist)), si);
        }
        if (W(d::f3dvec::z0) != 0) continue;   // don't move objects
        if (W(si + 24) == 0) continue;
        if (W(si + 52) <= 0) {   // the time ran out
            if (B(si + 65) != 0) {
                SB(M3(boomdeadly), 0x20);
                strtboom(si);
            }
            SW(si + 24, 0);
            continue;
        }
        SW(si + 52, sub16(W(si + 52), static_cast<i16>(m_refSteps)));
        SW(M3(objdisp), W(si + 60));
        SW(M3(objdamp), W(si + 62));
        SW(M3(tckgrav), static_cast<i16>(UW(si + 64) & 0x00FF));   // only the low byte has gravity
        objmov(si);
    }
}

// ==========================================================================================
// the starters: each makes pieces from a template at the position (and velocity) of object si
// ==========================================================================================

// the start every flotsam, shard and incendiary shares: a new object from the template tmpl at
// the origin of src (12 bytes), lifted off the ground a little, with the velocity of src
bool Main3::startPiece(u16 tmpl, u16 src, u16& neu) {
    SW(M3(expobj), static_cast<i16>(tmpl));
    if (m_world.expstrt(tmpl, neu)) return true;   // no room
    SW(M3(expobj), static_cast<i16>(neu));
    for (int k = 0; k < 6; k++) SW(neu + 2 * k, W(src + 2 * k));
    SW(neu + 8, add16(W(neu + 8), 750));   // lift off the ground
    for (int k = 0; k < 3; k++) SW(neu + 46 + 2 * k, W(src + 46 + 2 * k));
    return false;
}

// a random speed change: a value in [0, range) minus half the range (the ASM's randr / sar / sub)
i16 Main3::spread(u16 range) {
    return sub16(static_cast<i16>(m_rand.randr_w(range)), static_cast<i16>(static_cast<i16>(range) >> 1));
}

// strtflt: count pieces of flotsam (a random number of up to count unless it is a mortar's)
bool Main3::strtflt(u16 src, u16 count) {
    const u16 exprand = UW(M3(exprand));
    u16 cx;
    if (B(M3(flttype)) == 3) {
        cx = count;
    } else {
        const u16 half = static_cast<u16>(count >> 1);
        cx = static_cast<u16>(m_rand.randr_w(half) + half + 1);   // at least half get tried
    }
    for (; cx; cx--) {
        if (static_cast<u16>(UW(M3(nflt)) + cx) >= UW(M3(maxnflt))) {   // too many on the screen
            if (cx >= 26) m_eng.cecode = 163;   // an outlandish number was requested
            return false;
        }
        SW(M3(fltsm1) + 48, static_cast<i16>(m_rand.randr_w(4000)));   // a little random spin on the template
        u16 tmpl = static_cast<u16>(M3(fltsm2));
        if (B(M3(flttype)) != 3 && static_cast<i16>(m_rand.rand_w()) < 0) tmpl = static_cast<u16>(M3(fltsm1));
        u16 neu;
        if (startPiece(tmpl, src, neu)) return true;
        SW(neu + 46, add16(W(neu + 46), spread(exprand)));
        SW(neu + 48, add16(W(neu + 48), spread(exprand)));
        if (B(M3(omniflag)) == 0) SW(neu + 50, add16(W(neu + 50), static_cast<i16>(m_rand.randr_w(UW(M3(expzrand))))));
        else SW(neu + 50, add16(W(neu + 50), spread(exprand)));
        SB(neu + 18, 0);   // near
        const u16 time = B(M3(omniflag)) == 0 ? d::m3_equ::exptime : d::m3_equ::omnitime;
        const u16 base = static_cast<u16>(uwtadjRef(static_cast<i16>(time)));   // relative to machine speed
        const u16 life = static_cast<u16>(m_rand.randr_w(base) + (base >> 2));   // at least 1/4 of the time
        SW(neu + 52, static_cast<i16>(life));
        SW(neu + 24, static_cast<i16>(UW(neu + 24) | 0x8000));   // a timed object
        SB(neu + 19, B(M3(flttype)));
        if (B(M3(flttype)) == 3) {   // shot from a mortar: harmful to the plane
            SW(neu + 24, static_cast<i16>(UW(neu + 24) | 0x30));
            SB(neu + 65, 1);   // a wicked boom when it dies
        }
    }
    return false;
}

// strtincend: bx pieces of fire that fly up, spin and come down
bool Main3::strtincend(u16 src, u16 count) {
    for (u16 cx = count; cx; cx--) {
        u16 neu;
        if (startPiece(static_cast<u16>(M3(objincend)), src, neu)) return true;
        SW(neu + 46, spread(300));
        SW(neu + 48, spread(300));
        SW(neu + 50, static_cast<i16>(m_rand.randr_w(1200)));
        SB(neu + 18, 0);
        SW(neu + 52, uwtadjRef(150));   // live for 150 frames
        SW(neu + 24, static_cast<i16>(UW(neu + 24) | 0x8000));
        SW(neu + 54, static_cast<i16>(m_rand.randr_w(4000)));
        SW(neu + 56, static_cast<i16>(m_rand.randr_w(4000)));
        SW(neu + 58, static_cast<i16>(m_rand.randr_w(4000)));
    }
    return false;
}

// strtshard: pieces of debris that stay where they fall for about five minutes
bool Main3::strtshard(u16 src, u16 count) {
    const u16 exprand = UW(M3(exprand));
    const u16 half = static_cast<u16>(count >> 1);
    u16 cx = static_cast<u16>(m_rand.randr_w(half) + half + 1);
    for (; cx; cx--) {
        const u16 tmpl = static_cast<i16>(m_rand.rand_w()) < 0 ? static_cast<u16>(M3(objshard)) : static_cast<u16>(M3(objshard2));
        u16 neu;
        if (startPiece(tmpl, src, neu)) return true;
        const u16 quarter = static_cast<u16>(exprand >> 2);
        SW(neu + 46, add16(W(neu + 46), sub16(static_cast<i16>(m_rand.randr_w(quarter)), static_cast<i16>(quarter >> 1))));
        SW(neu + 48, add16(W(neu + 48), sub16(static_cast<i16>(m_rand.randr_w(quarter)), static_cast<i16>(quarter >> 1))));
        if (B(M3(omniflag)) == 0) SW(neu + 50, add16(W(neu + 50), static_cast<i16>(m_rand.randr_w(1500))));
        else SW(neu + 50, add16(W(neu + 50), spread(exprand)));
        SB(neu + 18, 0);
        SW(neu + 52, 6200);   // let them sit for about 5 minutes
        SW(neu + 24, static_cast<i16>(UW(neu + 24) | 0x8000));
        SW(neu + 54, static_cast<i16>(m_rand.randr_w(4000)));
        SW(neu + 56, static_cast<i16>(m_rand.randr_w(4000)));
        SW(neu + 58, static_cast<i16>(m_rand.randr_w(4000)));
    }
    return false;
}

// strtpuff: one to four puffs of smoke at the x and y of src, on the ground
bool Main3::strtpuff(u16 src) {
    for (u16 cx = static_cast<u16>(m_rand.randr_w(4) + 1); cx; cx--) {
        SW(M3(expobj), static_cast<i16>(M3(fltsm1puff)));
        u16 neu;
        if (m_world.expstrt(static_cast<u16>(M3(fltsm1puff)), neu)) return true;
        SW(M3(expobj), static_cast<i16>(neu));
        for (int k = 0; k < 4; k++) SW(neu + 2 * k, W(src + 2 * k));
        SW(neu + 8, 0);
        SW(neu + 10, 0);   // the z coordinate is zero
        SW(neu + 46, add16(W(neu + 46), spread(100)));
        SW(neu + 48, add16(W(neu + 48), spread(100)));
        SW(neu + 50, add16(W(neu + 50), static_cast<i16>(m_rand.randr_w(450))));
        SB(neu + 18, 0);
        SW(neu + 52, 7);   // one second
        SW(neu + 24, static_cast<i16>(UW(neu + 24) | 0x8000));
    }
    return false;
}

// strtboom: a ball of fire at src; boomdeadly (20h) says whether it hurts
bool Main3::strtboom(u16 src) {
    SW(M3(expobj), static_cast<i16>(M3(fltsm1boom)));
    u16 neu;
    if (m_world.expstrt(static_cast<u16>(M3(fltsm1boom)), neu)) return true;
    SW(M3(expobj), static_cast<i16>(neu));
    for (int k = 0; k < 6; k++) SW(neu + 2 * k, W(src + 2 * k));
    SW(neu + 46, add16(W(neu + 46), spread(100)));
    SW(neu + 48, add16(W(neu + 48), spread(100)));
    SW(neu + 50, add16(W(neu + 50), static_cast<i16>(m_rand.randr_w(450))));
    SB(neu + 18, 0);
    SW(neu + 52, d::m3_equ::bmtime);
    SW(neu + 24, static_cast<i16>(UW(neu + 24) | 0x8000));
    SB(neu + 24, static_cast<u8>(B(neu + 24) | B(M3(boomdeadly))));   // the destruction bit
    makeboom();
    return false;
}

// ==========================================================================================
// the routines of the pieces (objtimtable): carry = the object was hit or ran out
// ==========================================================================================

// flttim: flotsam disappears when it reaches the ground; a mortar's blows up when it dies
void Main3::flttim(u16 si, bool carry) {
    if (carry) {   // flt2hell
        const i16 n = sub16(W(M3(nflt)), 1);
        SW(M3(nflt), n < 0 ? 0 : n);
        if (B(si + 19) == 3) {
            SB(M3(boomdeadly), 0x20);
            strtboom(si);
            SW(si + 24, 0);   // remove from the universe
        }
        return;
    }
    if (W(si + 10) != 0 || W(si + 8) != 0) return;
    SW(si + 24, 0);   // cancel it when it hits the ground
}

// incendtim: fire explodes when it comes down (or is hit)
void Main3::incendtim(u16 si, bool carry) {
    if (!carry && W(si + 8) != 0) return;   // live and let fly while in the air
    SB(M3(boomdeadly), 0x20);
    strtboom(si);
    SB(M3(omniflag), 0);
    strtflt(si, 3);
    SW(si + 24, 0);
}

// shardtim: debris is black on the ground, and some of it burns red to white while it flies
void Main3::shardtim(u16 si, bool carry) {
    if (carry) {
        SW(si + 24, 0);
        return;
    }
    if (W(si + 8) == 0) {
        SW(M3(shrdclr), 0);
        return;
    }
    const int index = (si - static_cast<int>(EXPS)) / NBYTEXP;   // look at the two lower bits of the number
    if ((index & 3) == 0) SW(M3(shrdclr), static_cast<i16>(m_rand.randr_w(2) * 2 + 0x0C));   // oscillate from red to white
    else SW(M3(shrdclr), 0);
}

// boomtim: the fireball changes its picture over its first moments
void Main3::boomtim(u16 si) {
    const i16 left = W(si + 52);
    const i16 bmtime = d::m3_equ::bmtime;
    if (left > bmtime - 2) return;   // too long, the burn is over
    i16 pic = static_cast<i16>(M3(objflt2boom));
    if (!(left > bmtime - 3)) pic = static_cast<i16>(M3(objflt1boom));
    SW(si + 40, pic);
    SW(si + 42, pic);
    SW(si + 44, pic);
}

// ==========================================================================================
// the destroy paths of DEFOBJS.ASM that use the pieces
// ==========================================================================================

// the strength of the thing that hit: a bullet counts 1, a missile 6, a bomb 25 (di = 0xFFFF is the plane)
u8 Main3::hitStrength(u16 di) const {
    u8 bl = 1;
    if (B(static_cast<u16>(di + 24)) & 0x40) bl = 6;
    if (B(static_cast<u16>(di + 24)) & 0x80) bl = 25;
    return bl;
}

// fdestcheck: the hits left drop by the strength; true = it is destroyed
bool Main3::fdestcheck(u16 si, u16 di, bool planeKills) {
    if (planeKills && di == 0xFFFF) return true;   // the plane hit it (gmdest doesn't ask: it reads byte 23)
    const u8 bl = hitStrength(di);
    const int left = static_cast<int>(B(si + 27)) - bl;
    if (left <= 0) return true;
    SB(si + 27, static_cast<u8>(left));
    return false;
}

// qincfdest: counts the destruction of a friendly when it was the plane or our own ammo that did it
void Main3::qincfdest(u16 di) {
    bool inc = false;
    if (di == 0xFFFF) inc = true;
    else if (B(di + 19) != 0) inc = false;   // not one of our ammo items
    else if (B(di + 72) == 30 || B(di + 72) == 21) inc = true;
    else if (W(di + 72) == 20) inc = !(W(di + 24) & 2);   // a bomb, unless it came from a saucer
    if (inc) SW(M3(nfdest), add16(W(M3(nfdest)), 1));
}

// DEFOBJS.ASM fdest: a friendly takes a hit
void Main3::fdest(u16 si, u16 di, bool carry) {
    if (!carry) {
        SW(si + 52, add16(W(si + 52), static_cast<i16>(m_refSteps)));   // check our time on earth
        return;
    }
    if (!fdestcheck(si, di, true)) return;
    qincfdest(di);
    SB(si + 27, 0);   // clear the hits so they can't go negative
    SW(si + 8, static_cast<i16>(d::m3_equ::xgmx));
    SW(si + 10, 0);
    SW(si + 12, static_cast<i16>(m_rand.rand_w()));   // a random yaw angle
    SW(si + 40, static_cast<i16>(M3(wrkobj)));
    SW(si + 42, static_cast<i16>(M3(swrkobj)));
    SW(si + 44, static_cast<i16>(M3(swrkobj)));
    SW(si + 24, 0x11);   // non-destructable
    SW(si + 72, 33);     // a smoking thing
    SB(M3(omniflag), 0);
    strtflt(si, 12);
    strtshard(si, 5);
}

// fdestrdr: the radar turns (four times the clock), and otherwise is a friendly
void Main3::fdestrdr(u16 si, u16 di, bool carry) {
    SW(si + 12, static_cast<i16>(m_clock.oticks << 2));
    fdest(si, di, carry);
}

// gmdest: a ground missile site
void Main3::gmdest(u16 si, u16 di, bool carry) {
    if (!carry) {
        SW(si + 52, add16(W(si + 52), static_cast<i16>(m_refSteps)));   // check our time on earth
        SW(M3(curnticks), add16(W(M3(curnticks)), 1));
        return;
    }
    if (!fdestcheck(si, di, false)) return;
    SB(si + 27, 0);   // turn off hits, don't allow negative
    SW(si + 40, static_cast<i16>(M3(gmobj2)));
    SW(si + 42, static_cast<i16>(M3(gmobj3)));
    SW(si + 44, static_cast<i16>(M3(gmobj3)));
    SW(si + 24, 0x11);   // non-destructable
    SW(si + 72, 33);     // a smoking thing
    SW(M3(ndest), add16(W(M3(ndest)), 1));   // one more destroyed
    SB(M3(omniflag), 0);
    strtflt(si, 12);
    bigboom();
}

} // namespace corncob::game
