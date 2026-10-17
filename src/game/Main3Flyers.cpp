// Main3Flyers.cpp - Port of the rest of STRTEXP.ASM and the small object routines of DEFOBJS.ASM: the
// bombers (saucers) and their sites, the tetrahedra, the aliens, the portals, the shells that
// become a new object, the man on foot, the hut, and the rescue van's start
//
// Same conventions as Main3Sites.cpp: si = the object, di = what hit it (0xFFFF = the plane),
// carry = it is being destroyed.

#include "Main3.hpp"
#include "DGroup.hpp"
#include "core/AsmMath.hpp"
#include "engine/AsmTables.hpp"

namespace corncob::game {

namespace d = corncob::dgroup;
namespace lbl = corncob::lbl;
namespace st = corncob::dgroup::strtexp;
using asmx::hmul;
using engine::scos;
using engine::ssin;

#define M3(name) lbl::name

namespace {

inline i16 add16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) + static_cast<u16>(b))); }
inline i16 sub16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) - static_cast<u16>(b))); }
inline i16 neg16(i16 a) { return static_cast<i16>(static_cast<u16>(0u - static_cast<u16>(a))); }
inline i16 abs16(i16 a) { return a < 0 ? neg16(a) : a; }
inline u16 quality(u16 type) { return static_cast<u16>((type >> 12) & 7); }

} // namespace

// ==========================================================================================
// the bombers (saucers): strtbmr, the sites that make them, and the four modes of flying
// ==========================================================================================
bool Main3::strtbmr(u16 src) {
    u16 neu;
    const u16 tmpl = static_cast<u16>(W(d::defobjs::templatetable + 2 * 50));   // the identity code of a bomber
    SW(M3(expobj), static_cast<i16>(tmpl));
    if (m_world.expstrt(tmpl, neu)) return true;
    SW(M3(expobj), static_cast<i16>(neu));
    SB(neu + 18, 0);
    SW(neu + 52, 32000);
    u16 q = static_cast<u16>(UW(st::initialqual) & 7);
    q = static_cast<u16>((q >> 4) | (q << 12));
    SW(neu + 24, static_cast<i16>(UW(neu + 24) | q | 0x8000));
    SW(neu + 46, 0);
    SW(neu + 48, 0);
    SW(neu + 50, 0);
    SW(neu + 12, 0);
    SW(neu + 16, 0);
    SW(neu + 14, 0);
    for (int k = 0; k < 6; k++) SW(neu + 2 * k, W(src + 2 * k));
    return false;
}

// bmrstim: the site that makes bombers
void Main3::bmrstim(u16 si, u16 di, bool carry) {
    u8 qual;
    if (carry) {
        if (!enoughits(si, di)) {   // bmrstsaved
            const u16 q = getqtblptr(st::bmrstqtbl, si, qual);
            SB(M3(clrbmr), static_cast<u8>(UW(q)));
            SW(st::dbstrest, W(q + 4));
            resetdbst(si);
            return;
        }
        SB(M3(boomdeadly), 0);
        strtboom(si);
        SW(M3(ondest), add16(W(M3(ondest)), 1));
        getqtblptr(st::bmrstqtbl, si, qual);
        incW(M3(nsitesdest) + 2u * qual);
        SB(M3(omniflag), 1);
        strtflt(si, 9);
        SW(si + 24, 0);
        return;
    }
    incW(st::curnbmrst);
    const u16 q = getqtblptr(st::bmrstqtbl, si, qual);
    SB(M3(clrfss1), static_cast<u8>(UW(q)));
    SB(M3(clrfss2), static_cast<u8>(UW(q + 2)));
    SW(st::dbstrest, W(q + 4));
    if (!(UW(si + 52) < 100)) return;   // not due for another bomber
    bool make = UW(st::nbmrst) > UW(st::nbmrs);
    if (make) {
        const u16 bx = static_cast<u16>(wtadj(100));
        make = !(m_rand.rand_w() > bx);   // don't make one right away
    }
    if (!make) {
        SW(si + 52, add16(W(si + 52), 1));   // don't let us disappear
        return;
    }
    SW(st::initialqual, static_cast<i16>(quality(UW(si + 24))));
    strtbmr(si);
    incW(st::nbmrs);   // make sure we know the bomber is there
    resetdbst(si);
}

// startvector: a small vector in the direction of startcoords, for a nice bombing run; its size
// (the number of times it was halved) is how far away the target is
void Main3::startvector(u16 si) {
    SD(M3(dxvec), static_cast<i32>(static_cast<u32>(D(M3(startcoords))) - static_cast<u32>(D(si))));
    SD(M3(dxvec) + 4, static_cast<i32>(static_cast<u32>(D(M3(startcoords) + 4)) - static_cast<u32>(D(si + 4))));
    auto small = [&](i32 v) {
        const u32 a = v < 0 ? 0u - static_cast<u32>(v) : static_cast<u32>(v);
        return (a >> 16) == 0 && !(a & 0x8000) && (a & 0xFFFF) <= 10;
    };
    int cx = 31;
    for (; cx; cx--) {
        if (small(D(M3(dxvec))) && small(D(M3(dxvec) + 4))) break;
        SD(M3(dxvec), D(M3(dxvec)) >> 1);
        SD(M3(dxvec) + 4, D(M3(dxvec) + 4) >> 1);
    }
    SB(M3(clrlsph), 12);
    const int dist = 31 - cx;   // 0 to 31
    SW(st::dballlock, static_cast<i16>(dist));
    SW(st::bmrtargdist, static_cast<i16>(dist));
    if (dist <= 14) SB(M3(clrlsph), 0);   // black when close
}

// the four ways a bomber flies, picked by the mode bits of its type word
void Main3::bmrmodes(u16 si, u16 q, int mode) {
    const u32 dx = M3(dxvec);
    switch (mode) {
    case 0:   // famode: fly away from startcoords
        SB(M3(clrbmr), 0);
        SW(si + 46, sub16(W(si + 46), W(dx)));
        SW(si + 48, sub16(W(si + 48), W(dx + 4)));
        SW(st::bmrmodecprob, W(q + 14));
        break;
    case 2:   // circmode: fly around it
        SB(M3(clrbmr), 1);
        SW(si + 48, add16(W(si + 48), W(dx)));
        SW(si + 46, add16(W(si + 46), neg16(W(dx + 4))));
        SW(st::bmrmodecprob, W(q + 18));
        break;
    case 4: {   // atkmode: at it
        SB(M3(clrbmr), 2);
        SW(si + 46, add16(W(si + 46), static_cast<i16>(W(dx) * 2)));
        SW(si + 48, add16(W(si + 48), static_cast<i16>(W(dx + 4) * 2)));
        i16 ax = 3;
        if (B(si + 27) <= d::m3_equ::bmrhits / 2) ax = 20;   // hurting: four times more likely to break off
        SW(st::bmrmodecprob, ax);
        if (UW(st::bmrtargdist) <= 13) SW(st::bmrmodecprob, 50);   // leave the attack only when close to the target
        break;
    }
    default:  // slmode: just fly along
        SB(M3(clrbmr), 3);
        SW(st::bmrmodecprob, W(q + 20));
        break;
    }
}

// shootshells: a saucer shoots aa shells; it becomes a gun of the same quality (at most 5) for a moment
void Main3::shootshells(u16 si) {
    const u16 buf = static_cast<u16>(M3(mapobjbuf));
    for (int k = 0; k < 6; k++) SW(buf + 2 * k, W(si + 2 * k));   // the center coordinates
    for (int k = 0; k < 3; k++) SW(buf + 12 + 2 * k, 0);          // the angles
    u16 ax = UW(si + 24);
    u8 bl = static_cast<u8>(quality(ax));
    if (bl > 5) bl = 5;
    ax = static_cast<u16>((ax & ~0x7000) | (static_cast<u16>(bl) << 12));   // the new quality bits
    SW(buf + 24, static_cast<i16>(ax));
    SW(buf + 18, W(si + 18));   // the distance and the class
    SB(st::saucerflag, 0xFF);   // let the code know it's a saucer
    sauceraa(buf);
}

void Main3::bmrtim(u16 si, u16 di, bool carry) {
    u8 qual;
    if (carry) {
        SB(st::bhits, B(si + 27));   // the number of hits, kept for later
        bool destroyed = di == 0xFFFF || enoughits(si, di);
        if (!destroyed) {   // bmrsaved: a hit kicks the saucer over
            const int hit = static_cast<int8_t>(static_cast<u8>(B(st::bhits) - B(si + 27)));
            if (hit >= 0) SW(si + 16, add16(W(si + 16), static_cast<i16>(hit * 400)));
            return;
        }
        SW(M3(ndest), add16(W(M3(ndest)), 1));   // make it a resource
        SB(M3(boomdeadly), 0);                   // harmless
        strtboom(si);
        SB(M3(omniflag), 1);
        strtflt(si, 9);
        SW(si + 24, static_cast<i16>(UW(si + 24) & ~8));   // change us into a non-destructable object
        SW(si + 54, static_cast<i16>(m_rand.randr_w(4000)));   // it spins uncontrollably
        SW(si + 56, static_cast<i16>(m_rand.randr_w(4000)));
        SW(si + 58, static_cast<i16>(m_rand.randr_w(4000)));
        if (W(si + 72) == 53) incondest(si);   // the mission objective kind of saucer
        return;
    }
    incW(st::curnbmrs);
    const u16 q = getqtblptr(st::bmrqtbl, si, qual);
    SW(st::dballavgvel, W(q + 12));   // the default target speed
    SB(M3(clrfs1), static_cast<u8>(UW(q)));
    SB(M3(clrfss1), static_cast<u8>(UW(q)));
    SB(M3(clrfs2), static_cast<u8>(UW(q + 2)));
    SB(M3(clrfss2), static_cast<u8>(UW(q + 2)));
    u8 color = B(M3(clrfs2));
    if (m_clock.oticks & 512) color = B(M3(clrfs1));
    SB(M3(n), color);
    if (!radarprocess(si)) {   // a dot on the radar; find the nearest saucer (and how healthy it is)
        if ((W(si + 24) & 8) && !(UW(st::ssdist) > UW(st::saucerdist))) {
            SW(st::saucerdist, W(st::ssdist));
            const u16 dividend = static_cast<u16>(B(si + 27) << 8);
            SB(st::ssfreq, static_cast<u8>(dividend / (d::m3_equ::bmrhits + 1) + 2));   // health from 0 to 255, at least 2
        }
    }

    bool flyIt = true;   // false: skip the flying (abortnobmbdie)
    if (W(si + 72) == 53) {   // a mission objective
        SB(M3(clrfss1), B(M3(clrhq)));   // make the outer ring flash
        SB(M3(clrfss2), B(M3(clrhq)));
        if (UW(si + 52) >= 32700) {   // still in the resting state?
            if (!(W(si + 24) & 0x4000)) {   // a lower quality one just takes off
                SW(si + 52, 32000);
            } else if (B(si + 27) < d::m3_equ::bmrhits / 2) {   // takeoff: damaged ones stay on the ground
                SB(si + 27, d::m3_equ::bmrhits);   // repair the damage
                SW(si + 52, 32000);                // flag active
                incW(st::ndeadhqs);                // kill the shields
            } else {
                SW(si + 52, 32767);   // restore the timer value
                flyIt = false;
            }
        }
    }
    if (flyIt) {
        if (!(W(si + 24) & 8)) {   // shot down: it falls
            const i32 z = static_cast<i32>(static_cast<u32>(UW(si + 8)) | (static_cast<u32>(UW(si + 10)) << 16)) - 1000;
            if (z < 0) {   // hit the ground
                SB(M3(omniflag), 0);
                strtflt(si, 12);
                strtincend(si, 15);
                SW(si + 24, 0);
                bigboom();
            }
            SB(si + 64, 15);
            SW(si + 60, 20);
            SW(si + 62, 18000);
        } else {
            // itisalive: the time counts down (never below zero)
            i16 bx = add16(W(si + 52), 1);
            bx = sub16(bx, wtadj(3));
            if (bx < 0) bx = 0;
            SW(si + 52, bx);
            SW(si + 16, sub16(W(si + 16), static_cast<i16>(W(si + 16) >> 4)));   // an exponential fall off for the roll
            SW(si + 12, add16(W(si + 12), 4000));
            // if too low, rise up
            const u32 z32 = static_cast<u32>(UW(si + 8)) | (static_cast<u32>(UW(si + 10)) << 16);
            const u32 rel = z32 - (3u << 16);
            const i16 mid = static_cast<i16>((rel >> 8) & 0xFFFF);   // mov al,ah / mov ah,dl
            i16 ax = static_cast<i16>(neg16(mid) / 100);
            if (!(abs16(ax) < 40)) ax = ax > 15 ? 15 : (ax < -15 ? -15 : ax);   // bndck 15,-15
            ax = wtadj(ax);
            SW(si + 50, add16(W(si + 50), ax));
            i16 damp = uwtadj(20);   // the slower the machine, the more damping
            if (damp == 0) damp = 1;
            SW(si + 50, sub16(W(si + 50), static_cast<i16>(W(si + 50) / damp)));

            // flylevel
            startvector(si);
            const int mode = W(si + 24) & 6;
            SW(st::bmrmode, static_cast<i16>(mode));
            bmrmodes(si, q, mode);
            const u16 bx2 = static_cast<u16>(m_rand.rand_w() >> 1);
            if (!(bx2 > static_cast<u16>(wtadj(W(st::bmrmodecprob))))) {   // time for a mode switch
                const u16 next = static_cast<u16>((mode + 2) & 6);
                SW(si + 24, static_cast<i16>((UW(si + 24) & ~6) | next));
            }
            // drop a bomb if we can, shoot a death ball when far from the target
            if (UW(st::bmrtargdist) <= 13) {
                if (W(si + 10) != 0) {   // don't drop a bomb if too low
                    if (!(static_cast<u16>(m_rand.rand_w() >> 1) > UW(q + 22))) {
                        strtbomb1(si);   // drop a bomb from a flying saucer
                        const u16 bomb = static_cast<u16>(W(M3(expobj)));
                        SW(bomb + 24, static_cast<i16>(UW(bomb + 24) | 2));   // declare it a saucer bomb
                    }
                }
            } else if (!(static_cast<u16>(m_rand.rand_w() >> 1) > UW(q + 24))) {   // (the missile probability)
                if (W(si + 24) & 8) {   // don't shoot if the saucer is falling
                    if (!(B(M3(ndballs)) > B(q + 26))) {   // too many death balls already
                        SW(st::initialqual, static_cast<i16>(quality(UW(si + 24))));   // the ball's quality equals ours
                        incW(M3(ndballs));
                        const i16 savedQ = W(st::qtblptr);
                        strtdball(si);
                        SW(st::qtblptr, savedQ);
                        const u16 ball = static_cast<u16>(W(M3(expobj)));   // make the new ball fall
                        SB(ball + 64, 20);
                        SW(ball + 60, 20);
                        SW(ball + 62, 16000);
                        SW(ball + 52, add16(uwtadj(60), static_cast<i16>(d::m3_equ::dballtime2)));   // inactive for 3 seconds
                    }
                }
            }
            // norandbomb: jinking and serpentine flight
            i16 t = static_cast<i16>(32000 - W(si + 52));
            if (!(static_cast<u16>(t) < 1000)) {
                SW(si + 52, 32000);
                t = 0;
            }
            t = wtadj(t);
            if (W(q + 6) != 0) {   // do we jink?
                const u16 period = static_cast<u16>(UW(q + 4) << 3);
                const u32 prod = static_cast<u32>(static_cast<u16>(t)) * period;
                const i16 s = neg16(ssin(static_cast<i16>(prod & 0xFFFF)));
                const i16 dxj = hmul(s, W(q + 6));
                if (W(si + 10) != 0) SW(si + 50, add16(W(si + 50), dxj));   // some sin in the height
            }
            if (W(q + 10) != 0) {   // do we serpentine?
                const u16 period = static_cast<u16>(UW(q + 8) << 3);
                const u16 arg = static_cast<u16>((static_cast<u32>(static_cast<u16>(t)) * period) & 0xFFFF);
                const i16 sx = neg16(ssin(static_cast<i16>(arg)));
                SW(si + 46, add16(W(si + 46), hmul(sx, W(q + 10))));
                const i16 cy = neg16(scos(static_cast<i16>(arg)));
                SW(si + 48, add16(W(si + 48), hmul(cy, 100)));
            }
            // gowherepoint: keep the speed from growing too much
            const i16 oldvel = static_cast<i16>(vmag(static_cast<u16>(si + 46)));
            SW(st::olddballvel, oldvel);
            SW(st::dballvel, oldvel);
            adjdballv(q);
            i16 num = W(st::dballvel);
            i16 den = oldvel;
            if (!(static_cast<u16>(num) > static_cast<u16>(den)) && den != 0) {   // only slowing is applied
                if (!(num <= static_cast<i16>(den * 2))) {
                    num = 2;   // then just a factor of two
                    den = 1;
                }
                for (int k = 0; k < 3; k++) SW(si + 46 + 2 * k, static_cast<i16>((static_cast<i32>(W(si + 46 + 2 * k)) * num / den) & 0xFFFF));
            }
        }
    }
    // abortnobmbdie: an armed saucer of a shooting type shoots shells
    if ((W(si + 24) & 8) && B(q + 27) != 0) shootshells(si);
}

// ==========================================================================================
// the tetrahedra, the aliens, the portals
// ==========================================================================================
void Main3::stettim(u16 si, u16 di, bool carry) {
    if (carry) {
        if (W(di + 72) != 20 && W(di + 72) != 32) return;   // only a bomb or a crater blows it
        if (W(si + 24) & 0x800) {   // a special tet needs an assassin bomb
            if (!(W(di + 24) & 0x800)) return;
        }
        if (!enoughits(si, di)) return;
        SB(M3(boomdeadly), 0x20);   // deadly
        strtboom(si);
        SB(M3(omniflag), 1);
        strtshard(si, 3);
        SW(si + 24, 0);
        SB(M3(tripflag), static_cast<u8>(B(M3(tripflag)) & ~1));   // kill the trip
        for (int k = 0; k < 3; k++) SW(M3(haluclrs) + 2 * k, 0);
        m_eng.distcolors();
        return;
    }
    SW(si + 52, add16(W(si + 52), 1));   // the object lasts forever
    incW(st::curnstets);
    if (getobsvec(si, true) || UW(M3(dballdist)) > 1000) {   // we want the distance to us, not the plane
        SB(M3(tripflag), static_cast<u8>(B(M3(tripflag)) & ~1));
        return;
    }
    SW(si + 24, static_cast<i16>(UW(si + 24) | 4));   // turn on the weird effect
    const u32 near = 1000u - UW(M3(dballdist));       // 0 if far, 1000 if close
    SW(M3(palmul), static_cast<i16>(near * near / 31));   // 0-1000 squared scaled into 0-32258
    SB(M3(tripflag), static_cast<u8>(B(M3(tripflag)) | 1));   // trip it out
}

void Main3::tettim(u16 si, u16 di, bool carry) {
    if (!carry) {
        SW(si + 52, add16(W(si + 52), 1));   // the object lasts forever
        incW(st::curntets);
        return;
    }
    if (enoughits(si, di)) {
        SB(M3(boomdeadly), 0x20);
        strtboom(si);
        SB(M3(omniflag), 1);
        strtshard(si, 3);
        SW(M3(ndest), add16(W(M3(ndest)), 1));
        SW(si + 24, 0);
    }
    SB(M3(omniflag), 1);   // (tetsaved) an all around explosion either way
    strtflt(si, 2);
}

// hoparound: a value moved around at random, and pulled back by an eighth
i16 Main3::hoparound(i16 ax, u16 bx) {
    i16 v = sub16(static_cast<i16>(m_rand.randr_w(bx)), static_cast<i16>(bx >> 1));
    v = add16(v, ax);
    return sub16(v, static_cast<i16>(v >> 3));
}

void Main3::altim(u16 si, u16 di, bool carry) {
    if (carry) {
        if (W(di + 72) != 30) return;   // only bullets hurt
        if (!enoughits(si, di)) return;
        SW(si + 8, 0);
        SW(si + 10, 0);
        SW(si + 14, 16384);   // lie him down
        SW(si + 24, static_cast<i16>(UW(si + 24) & ~0x0C));   // make destructable
        SB(si + 64, 0);       // turn off gravity
        SW(si + 50, 100);     // make them float up
        SW(M3(ndest), add16(W(M3(ndest)), 1));   // count them as a resource point
        return;
    }
    u8 qual;
    const u16 q = getqtblptr(st::alqtbl, si, qual);
    SB(M3(alclr), static_cast<u8>(UW(q)));
    SW(si + 52, add16(W(si + 52), 1));   // mr alien never dies
    if (!(W(si + 24) & 4)) {             // not destructable: gone when out of range
        if (B(si + 18) != 0) SW(si + 24, 0);
        return;
    }
    incW(st::curnals);
    if (B(M3(ejectflag)) != 0 && B(si + 18) == 0) {   // the man is out and near: activate the alien
        SW(si + 52, 16000);
        SW(si + 24, static_cast<i16>(UW(si + 24) | 8));   // make it destructable
    }
    if (W(si + 52) & 16384) return;   // not activated
    if (!(W(si + 24) & 8)) return;    // not destructable
    if (W(si + 10) >= 0 && (W(si + 10) | W(si + 8)) != 0) return;   // in the air
    SW(si + 50, W(q + 4));   // jump up
    SW(si + 8, 0);
    SW(si + 10, 0);
    SB(si + 64, static_cast<u8>(UW(q + 2)));   // gravity on
    SW(si + 46, hoparound(W(si + 46), UW(q + 6)));   // hop around a little
    SW(si + 48, hoparound(W(si + 48), 50));
    SW(si + 12, hoparound(W(si + 12), 1000));
}

// qxpdamage: does the plane deserve damage from this portal? (bx: 0 none, negative kills, bit 1 repairs)
void Main3::qxpdamage(i16 bx) {
    if (bx == 0) return;
    if (bx < 0) {
        SB(M3(crshflg), 0xFF);
        return;
    }
    if (bx & 2) {
        cleardamage();
        restoreview();   // clear the damage indicators
        return;
    }
    damageplane(75);
}

void Main3::xptim(u16 si, u16 di, bool carry) {
    u8 qual;
    if (carry) {
        if (di == 0xFFFF && B(M3(ejectflag)) == 0) {   // the plane hit it
            const u16 q = getqtblptr(st::xpqtbl, si, qual);
            SB(st::xpflag, static_cast<u8>(UW(q + 4) & 0xFF));
            if ((UW(q + 4) >> 8) & 1) incondest(si);   // the user wants it to complete the mission
            qxpdamage(W(q + 6));
        }
        SW(si + 52, static_cast<i16>(UW(si + 52) & 16383));   // cut out that top bit or so
        return;
    }
    const u16 q = getqtblptr(st::xpqtbl, si, qual);
    u8 color = static_cast<u8>(UW(q));
    if (W(si + 24) & 0x200) color = 15;   // completed: white
    SB(M3(xpclr), color);
    const u8 alt = static_cast<u8>(UW(q + 2));
    if (static_cast<i16>(m_rand.rand_w()) >= 0) SB(M3(xpclr), alt);
    const u16 time = UW(si + 52);
    SW(si + 52, 32000);   // set the timer back
    if (!(time & 16384)) SB(M3(xpclr), alt);   // flash that sucker
}

// ==========================================================================================
// the small ones: shells that become an object, the man, the hut, the survivors' markers
// ==========================================================================================

// shelltim: the shell (a tree, a building ...) that turns into another object when destroyed
void Main3::shelltim(u16 si, bool carry) {
    u8 qual;
    if (carry) {
        const u16 kbuf = static_cast<u16>(d::m3::kbuf);
        for (int k = 0; k < 9; k++) SW(kbuf + 2 * k, W(si + 2 * k));   // the position and angles
        u16 q = getqtblptr(st::shellqtbl, si, qual);
        const u16 tmpl = static_cast<u16>(W(d::defobjs::templatetable + 2u * UW(q)));   // the object to make
        SW(kbuf + 8, W(tmpl + 8));    // the default height of that object
        SW(kbuf + 10, W(si + 10));
        SW(st::initialqual, 7);
        u16 slot;
        m_world.strtperm(kbuf, tmpl, 7, slot);
        SW(si + 24, 0);   // cancel the present object
    }
    const u16 q = getqtblptr(st::shellqtbl, si, qual);
    if (qual == 6) incW(st::curnticks);
    SB(M3(clrshell), 0);   // force the color to be black
    const u16 time = UW(si + 52);
    if (time & 16384) {   // native mode: how many frames elapsed
        const u16 elapsed = static_cast<u16>(32000 - time);
        if (static_cast<u16>(uwtadj(30)) < elapsed) SW(si + 52, 16000);   // change the color mode
        return;
    }
    SB(M3(clrshell), static_cast<u8>(UW(q + 4)));
    const u16 elapsed = static_cast<u16>(16000 - time);
    u16 limitFrames = static_cast<u16>(uwtadj(5));
    if (!(limitFrames > 2)) limitFrames = 2;
    if (limitFrames < elapsed) SW(si + 52, 32000);
}

// mandest: the man (a spy, or a survivor): killed by a hit, fingered when we walk up to him
void Main3::mandest(u16 si, u16 di, bool carry) {
    if (carry) {
        if (W(si + 24) & 0x1000) return;   // already dead
        const u16 ext = static_cast<u16>(W(d::defobjs::extendedtypetbl + 2u * UW(di + 72)));
        if (ext & 8) return;   // a forcefield: don't kill the guy
        if (!fdestcheck(si, di, true)) return;
        SW(si + 24, static_cast<i16>(UW(si + 24) | 0x1000));   // a dead man
        SW(si + 24, static_cast<i16>(UW(si + 24) & ~8));       // indestructable
        qincfdest(di);
        return;
    }
    SB(M3(pantscolor), 1);
    SB(M3(shirtcolor), 6);
    if (W(si + 24) & 0x800) {   // a spy wears black
        SB(M3(pantscolor), 0);
        SB(M3(shirtcolor), 0);
    }
    if (W(si + 24) & 0x1000) {   // dead: lie down on the ground
        const i16 dead = static_cast<i16>(M3(dmanobj));
        SW(si + 40, dead);
        SW(si + 42, dead);
        SW(si + 44, dead);
        SW(si + 14, 16384);
    }
    SW(si + 52, 32000);   // never die
    if (m_eng.rel3d(si) || m_eng.rangeBits() != 0) return;   // too far away
    if (B(M3(ejectflag)) == 0) return;   // if the pilot is in the plane, don't finger
    if (W(si + 40) != static_cast<i16>(M3(manobj))) return;   // a dead man is not fingered
    SW(M3(manvec), W(M3(dxvec)));
    SW(M3(manvec) + 2, W(M3(dxvec) + 4));
    SW(M3(manvec) + 4, W(M3(dxvec) + 8));
    if (vmag(static_cast<u16>(M3(manvec))) > 2000) return;
    const i16 fingered = static_cast<i16>(M3(fmanobj));
    SW(si + 40, fingered);
    SW(si + 42, fingered);
    SW(si + 44, fingered);
    if (W(si + 24) & 0x800) incondest(si);   // a spy: one more objective
}

// huttim: the hut; walking up to it when on foot goes into the tower
void Main3::huttim(u16 si, bool carry) {
    if (carry) return;
    SW(si + 52, 32000);   // never die
    if (B(M3(ejectflag)) == 0) return;   // don't allow tower access to the plane
    u16 dist;
    if (!evaldist(si, dist) || dist > 2000) return;
    SB(M3(redamageflag), 0);
    SB(M3(intowerflag), 0xFF);
}

// rescuestart: the rescue van starts at the start coordinates and works its way into range
void Main3::rescuestart() {
    const u16 kbuf = static_cast<u16>(d::m3::kbuf);
    for (int k = 0; k < 4; k++) SW(kbuf + 2 * k, W(M3(startcoords) + 2 * k));
    SW(st::initialqual, 7);
    SW(kbuf + 8, 50);
    SW(kbuf + 10, 0);
    SW(kbuf + 12, 0);
    SW(kbuf + 14, 0);
    SW(kbuf + 16, 0);
    u16 si;
    if (m_world.strtperm(kbuf, static_cast<u16>(M3(objrv)), 7, si)) return;
    SB(si + 18, 3);   // set up the default distance
    for (int cx = 16; cx; cx--) {
        if ((B(si + 18) & 0xFE) == 0) return;   // close enough
        m_eng.disteval(si);   // dxvec points the way
        SD(si, static_cast<i32>(static_cast<u32>(D(si)) - static_cast<u32>(D(M3(dxvec)) >> 1)));   // half the distance closer
        SD(si + 4, static_cast<i32>(static_cast<u32>(D(si + 4)) - static_cast<u32>(D(M3(dxvec) + 4) >> 1)));
    }
    m_eng.cecode = 75;   // an error if it never became close
}

} // namespace corncob::game
