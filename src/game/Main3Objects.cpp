// Main3Objects.cpp - Port of the object routines the main loop needs: planes, towers, runways,
// the plane's destruction, getting into a plane, damage; and the small view procs of 3.ASM
//
// The type -> routine table is DEFOBJS.ASM's objtimtable. Types whose routines belong to the
// combat code (HQs, vehicles, missiles, flotsam, shards ...) live in Main3Timed, Main3Weapons, Main3Sites and
// Main3Flyers.cpp; the table here sends each type to its routine.

#include "Main3.hpp"
#include "DGroup.hpp"
#include "core/AsmMath.hpp"
#include "engine/AsmTables.hpp"
#include <algorithm>
#include <cstdlib>

namespace corncob::game {

namespace d = corncob::dgroup;
using asmx::bndck;
using asmx::hmul;
using asmx::idiv32;
using asmx::imul16;
using engine::atn2;
namespace lbl = corncob::lbl;

#define M3(name) lbl::name

namespace {

inline i16 add16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) + static_cast<u16>(b))); }
inline i16 sub16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) - static_cast<u16>(b))); }
inline i16 neg16(i16 a) { return static_cast<i16>(static_cast<u16>(0u - static_cast<u16>(a))); }
inline i16 sar16(i16 a, int n) { return static_cast<i16>(a >> n); }
inline i32 add32(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b)); }
inline u32 mul16(u16 a, u16 b) { return static_cast<u32>(a) * b; }
inline i16 hi16(i32 v) { return static_cast<i16>(static_cast<u32>(v) >> 16); }

} // namespace

// ==========================================================================================
// GameHooks: the objtimtable (DEFOBJS.ASM) and 3.ASM's damageplane
// ==========================================================================================
void Main3::objroutine(u16 type, u16 si, u16 di, bool carry) {
    switch (type) {
    case 2:   // obj5da: a plane on the field
        obj5datim(si, di, carry);
        break;
    case 4:   // donothing: a permanent object
        SW(si + 52, 32000);
        break;
    case 5:   // roadtim: a runway piece
        SW(si + 52, 32000);
        if (UW(si + 24) & 0x7000) {   // a paved piece (quality bits)
            const i16 pave = static_cast<i16>(M3(paveobj));
            SW(si + 40, pave);
            SW(si + 42, pave);
            SW(si + 44, static_cast<i16>(M3(sroadobj)));
        }
        break;
    case 6:   // towertim: the control tower: home base detection, and where a new start is
        towertim(si, carry);
        break;
    case 28:  // odest: the plane the pilot flies
        odest(si, di, carry);
        break;
    case 3:   // fdestrdr: the radar
        fdestrdr(si, di, carry);
        break;
    case 8:   // shardtim
    case 9:
        shardtim(si, carry);
        break;
    case 10:  // incendtim
        incendtim(si, carry);
        break;
    case 18:  // pufftim: nothing
        break;
    case 19:  // boomtim
        boomtim(si);
        break;
    case 25:  // flttim
    case 26:
        flttim(si, carry);
        break;
    case 27:  // gmdest: a ground missile site
        gmdest(si, di, carry);
        break;
    case 20:  // bombtim
        bombtim(si, carry);
        break;
    case 21:  // mistim
        mistim(si, carry);
        break;
    case 30:  // bdest: a bullet
        bdest(si, di, carry);
        break;
    case 32:  // crtrtim: a crater
        crtrtim(si, carry);
        break;
    case 29:  // donothing
    case 33:
    case 35: case 36: case 37: case 38: case 39:
        SW(si + 52, 32000);   // don't let it run down
        break;
    case 0:   // bigdest: the headquarters orb
        bigdest(si, di, carry);
        break;
    case 1:   // stettim: a tetrahedron of the special kind
        stettim(si, di, carry);
        break;
    case 7:   // mandest: the man
        mandest(si, di, carry);
        break;
    case 11:  // dbsttim: a death ball site
        dbsttim(si, di, carry);
        break;
    case 12:  // vechtim: a vehicle
    case 48:
        vechtim(si, di, carry);
        break;
    case 13:  // vesttim: a vehicle site
        vesttim(si, di, carry);
        break;
    case 14: case 15: case 16: case 17:   // walltim
        walltim(si, carry);
        break;
    case 22:  // aashelltim: an anti-aircraft shell
        aashelltim(si, carry);
        break;
    case 23:  // aatim: an anti-aircraft gun
        aatim(si, di, carry);
        break;
    case 24:  // dballtim: a death ball
        dballtim(si, carry);
        break;
    case 31:  // survtim: a survey marker, permanent
        SW(si + 52, 32000);
        break;
    case 34:  // huttim: the hut
        huttim(si, carry);
        break;
    case 40:  // bbtim: a barrage balloon
        bbtim(si, carry);
        break;
    case 41:  // bbsmtim: a bee swarm
        bbsmtim(si, carry);
        break;
    case 42:  // mortim: a mortar
        mortim(si, di, carry);
        break;
    case 43:  // dhqtim: the wreck of an orb
        dhqtim(si);
        break;
    case 44: case 45: case 46: case 47:   // lwalltim
    case 54: case 55: case 56: case 59: case 60:
        lwalltim(si, carry);
        break;
    case 49:  // shelltim
        shelltim(si, carry);
        break;
    case 50:  // bmrtim: a bomber
    case 53:
        bmrtim(si, di, carry);
        break;
    case 51:  // bmrstim: a bomber site
        bmrstim(si, di, carry);
        break;
    case 52:  // tettim: a tetrahedron
        tettim(si, di, carry);
        break;
    case 57:  // altim: an alien
        altim(si, di, carry);
        break;
    case 58:  // xptim: a portal
        xptim(si, di, carry);
        break;
    default:
        break;
    }
}

// 3.ASM damageplane: damages the plane in a random fashion, count times
void Main3::damageplane(u16 count) {
    if (B(M3(invincible)) != 0) return;
    if (UW(M3(totaldamage)) > 200) return;   // don't go overboard
    for (u16 cx = count; cx; cx--) {
        SB(M3(shakecnt), 6);   // shake the plane
        SW(M3(totaldamage), add16(W(M3(totaldamage)), 1));
        switch (m_rand.randr_w(4)) {   // four types of damage
        case 0: engdamage(); break;
        case 1: wingdam(); break;
        case 2: stabdamage(); break;
        default: yawdamage(); break;
        }
    }
    if (UW(M3(maxtotaldamage)) < UW(M3(totaldamage))) SW(M3(maxtotaldamage), W(M3(totaldamage)));
}

void Main3::wingdam() {
    if (static_cast<u16>(std::abs(static_cast<int>(W(M3(thrdam))))) > 80 * 20) return;   // no more than 20 hits worth
    drawdamage(1);
    i16 dx = uwtadjRef(W(M3(thrdamunit)));   // lessen the effect on slower machines
    if (static_cast<i16>(m_rand.rand_w()) < 0) dx = neg16(dx);   // affect a random wing
    SW(M3(thrdam), add16(W(M3(thrdam)), dx));
    SW(M3(wingloss), add16(W(M3(wingloss)), W(M3(winglossunit))));   // put a hole in the lift
}

void Main3::yawdamage() {
    if (static_cast<u16>(std::abs(static_cast<int>(W(M3(yawdam))))) > 15 * 20) return;
    drawdamage(3);
    i16 dx = uwtadjRef(W(M3(yawdamunit)));
    i16 ax = static_cast<i16>(m_rand.rand_w());   // affect a random side
    if (W(M3(yawdam)) != 0) ax = W(M3(yawdam));   // (damage already: the sign follows it)
    if (ax < 0) dx = neg16(dx);
    SW(M3(yawdam), add16(W(M3(yawdam)), dx));
}

void Main3::stabdamage() {
    SW(M3(stabdam), add16(W(M3(stabdam)), uwtadjRef(W(M3(stabunit)))));
    drawdamage(2);
}

void Main3::engdamage() {
    SW(M3(engdam), add16(W(M3(engdam)), 1));   // amount of damage to the engine
    const u32 prod = mul16(UW(M3(currpmx)), UW(M3(engdamunit)));
    const u16 ax = static_cast<u16>(prod / 1000);
    if (ax < 20) return;   // lowest available throttle
    SW(M3(currpmx), static_cast<i16>(ax));
    if (ax < UW(M3(rpm))) SW(M3(rpm), static_cast<i16>(ax));   // rpm above the max comes down
    drawdamage(0);
}

// 3.ASM cleardamage
void Main3::cleardamage() {
    SW(M3(thrdam), 0);
    SW(M3(stabdam), 0);
    SW(M3(engdam), 0);
    SW(M3(yawdam), 0);
    SW(M3(wingloss), 0);
    SW(M3(totaldamage), 0);
    SB(M3(redamageflag), 0);   // do not use old damage
    SW(M3(currpmx), 200);
    SB(M3(brdrclr), 7);        // clear the border color
}

// ==========================================================================================
// DEFOBJS.ASM: evaldist, the plane (obj5datim), odest, funeral5d, towers
// ==========================================================================================

// DEFOBJS.ASM evaldist: how far is this object from the observer; false when too far
bool Main3::evaldist(u16 si, u16& dist) {
    if (m_eng.rel3d(si)) return false;
    const u16 cx = m_eng.rangeBits();
    if (cx >> 8) return false;       // if ch is not zero, then too far
    if (cx & 0xF0) return false;     // still too far
    SW(M3(manvec), W(M3(dxvec)));
    SW(M3(manvec) + 2, W(M3(dxvec) + 4));
    SW(M3(manvec) + 4, W(M3(dxvec) + 8));
    dist = vmag(static_cast<u16>(M3(manvec)));
    return true;
}

// DEFOBJS.ASM placeplane: the planes line up behind the start of the runway
void Main3::placeplane(u16 si) {
    for (int k = 0; k < 8; k++) SW(si + 2 * k, W(M3(startcoords) + 2 * k));   // x, y, z and the yaw
    const u32 back = mul16(static_cast<u16>(1800 * 4), UW(M3(nplaced)));   // l10*4 for each plane placed
    SD(si, static_cast<i32>(static_cast<u32>(D(si)) - back));
    SW(si + 12, -8000);   // so the plane points the right way
    SW(M3(nplaced), add16(W(M3(nplaced)), 1));
}

// DEFOBJS.ASM obj5datim: what a plane on the field does. carry = destroy it.
void Main3::obj5datim(u16 si, u16 di, bool carry) {
    if (carry) {   // blastus
        if (di == static_cast<u16>(M3(obj5d))) return;   // object 5d never kills us
        const u16 ext = static_cast<u16>(W(M3(extendedtypetbl) + 2u * static_cast<u16>(W(di + 72))));
        if (ext & 16) return;                              // it is a runway: no kill
        fdest(si, di, true);                               // a friendly destruction
        return;
    }
    if (W(si + 24) & 0x100) {   // crashed: a smoking wreck
        const i16 sm = static_cast<i16>(M3(plnplyobjsm));
        SW(si + 40, sm);
        SW(si + 42, sm);
    }
    SB(M3(wingclr), 1);
    if (W(si + 24) & 0x1000) SB(M3(wingclr), 6);   // brown wings for the dumpy plane
    SW(si + 52, 32000);   // never die

    bool fiddled = false;
    if (B(M3(exitflag)) == 0) {   // if we are leaving, don't get in the plane
        bool toFiddled = false;
        if (B(M3(resetplanesflag)) != 0) {
            SB(si + 65, 0);
            toFiddled = true;
        } else if (B(si + 65) & 0x10) {
            toFiddled = true;
        } else if (W(si + 24) & 0x2100) {   // crashed, or a bomber
            toFiddled = true;
        } else {
            const u16 bits = static_cast<u16>(W(si + 24) & 6);
            if (bits != 6 && bits != 0) {
                toFiddled = true;            // not a plane to repair
            } else {
                SW(si + 24, static_cast<i16>(UW(si + 24) & ~0x106));   // repair the plane
                for (int k = 0; k < 5; k++) SW(si + 8 + 2 * k, W(M3(startcoords) + 8 + 2 * k));   // z and the angles
                if (!(B(si + 65) & 1)) {   // have we placed this plane at the start?
                    placeplane(si);
                    SB(si + 65, B(si + 65) | 1);
                    toFiddled = true;
                } else if (B(M3(ejectflag)) != 0 && B(M3(newguyflag)) != 0) {
                    // the new guy gets into the first plane he sees
                    u16 dist;
                    if (evaldist(si, dist)) {
                        SB(M3(newguyflag), 0);
                        SD(si + 4, static_cast<i32>(static_cast<u32>(D(si + 4)) - 8000));   // beginning of the runway
                        SW(si + 12, 0);                                                      // point exactly north
                        getinplane(si);
                        SW(si + 24, 0);
                        return;
                    }
                    toFiddled = true;
                } else {
                    toFiddled = true;
                }
            }
        }
        fiddled = toFiddled;
    } else {
        fiddled = true;
    }
    (void)fiddled;
    // hasbeenfiddled: walking up to the plane gets us in
    if (B(M3(ejectflag)) != 0 && B(M3(exitflag)) == 0) {
        u16 dist;
        if (evaldist(si, dist) && dist <= 2000) {
            getinplane(si);
            SW(si + 24, 0);   // remove this object from the table
            return;
        }
    }
    if (W(si + 24) & 0x2000) {   // a bomber
        const i16 st = static_cast<i16>(M3(stlhobj));
        SW(si + 40, st);
        SW(si + 42, st);
    }
}

// DEFOBJS.ASM odest: the plane the pilot flies
void Main3::odest(u16 si, u16 di, bool carry) {
    if (!carry) {
        SW(si + 52, 32000);
        if (si != static_cast<u16>(M3(obj5d))) {
            SW(si + 72, 2);   // make into an obj5da
            obj5datim(si, di, false);
            return;
        }
        SB(M3(wingclr), 1);
        if (W(si + 24) & 0x1000) SB(M3(wingclr), 6);   // brown wings for the dumpy plane
        if (W(si + 24) & 0x2000) {                       // the bomber looks different
            const i16 st = static_cast<i16>(M3(stlhobj));
            SW(si + 40, st);
            SW(si + 42, st);
        }
        return;
    }
    // realodest: the plane is destroyed
    SW(si + 24, 1);   // make the object non-destructable
    SW(si + 10, 0);
    SW(si + 46, 0);   // clear the height and velocity
    SW(si + 48, 0);
    SW(si + 50, 0);
    SW(si + 8, 100);
    SB(M3(omniflag), 0);
    strtflt(si, 15);
    strtshard(si, 15);
    SW(si + 8, 0);
    SW(si + 10, 0);
    SB(M3(freezeflag), 0xFF);
    SW(M3(rpm), 20);
    funeral5d(true);   // replace obj5d with an obj5da
    bigboom();         // plane crash
}

// DEFOBJS.ASM funeral5d: takes out object 5d and starts a nice parked plane
void Main3::funeral5d(bool wrecked) {
    SW(M3(obj5d) + 24, 0);   // have object 5d removed from the table
    const u32 kbuf = d::m3::kbuf;
    for (int k = 0; k < 9; k++) SW(kbuf + 2 * k, W(M3(obj5d) + 2 * k));   // transfer all angles and stuff
    const u16 da = static_cast<u16>(M3(obj5da));
    SW(da + 46, 0);
    SW(da + 48, 0);
    SW(da + 50, 0);   // put the velocity to zero
    u16 slot;
    if (m_world.strtperm(static_cast<u16>(kbuf), da, 0, slot)) return;   // no room for another object
    const u16 si = slot;
    SW(si + 24, static_cast<i16>(UW(si + 24) | UW(M3(obj5dbits))));   // crashed and home bits from obj5d
    SB(si + 65, B(si + 65) | 0x10);                                    // mark the plane as being fiddled with
    if (wrecked) {
        SW(si + 24, static_cast<i16>(UW(si + 24) | 0x100));    // make it into a wall
        SW(si + 24, static_cast<i16>(UW(si + 24) & ~8));       // and indestructable
    }
}

// DEFOBJS.ASM towertim: home base detection, and where new missions start
void Main3::towertim(u16 si, bool carry) {
    homedetect(si);
    // the start coordinates follow the tower: add ax,3001 / adc dx,-2 and add ax,-16000 / adc dx,-1
    SD(M3(startcoords), add32(D(si), static_cast<i32>(0xFFFE0000u | 3001u)));
    SD(M3(startcoords) + 4, add32(D(si + 4), -16000));
    // curtower: which of the 3x3 areas of the universe the tower is in
    const i32 tx = static_cast<i32>(static_cast<u32>(D(si)) - 0xFFF4FFFFu);   // sub ax,-1 / sbb dx,roadx-2
    i16 dx = static_cast<i16>(hi16(tx) - W(d::f3dvec::universex));
    dx = sub16(static_cast<i16>(static_cast<u16>(dx) >> 6), 1);                  // now 0, 1 or 2
    const i32 ty = static_cast<i32>(static_cast<u32>(D(si + 4)) - 16000u);
    i16 dy = static_cast<i16>(hi16(ty) - W(d::f3dvec::universey));
    dy = sub16(static_cast<i16>(static_cast<u16>(dy) >> 6), 1);
    SW(M3(curtower), add16(static_cast<i16>(static_cast<u16>(dy) * 3u), dx));      // 0, 3 or 6 plus the column
    fdest(si, 0, carry);
}

// STRTEXP.ASM homedetect: how far is the observer from the tower of the air base: are we at home?
void Main3::homedetect(u16 si) {
    SB(M3(closetowerflag), 0);   // far until proven close
    bool nohome = m_eng.rel3d(si);
    if (!nohome) {
        const u16 cx = m_eng.rangeBits();
        nohome = (cx >> 8) != 0 || (cx & 0xF0) != 0;
    }
    if (!nohome) {   // the vector to the observer, divided by 64: its length is the distance
        for (int k = 0; k < 3; k++) {
            const i32 dd = D(M3(dxvec) + 4 * k) >> 6;
            SW(M3(dtopv) + 2 * k, neg16(static_cast<i16>(dd & 0xFFFF)));
        }
        SW(M3(dballdist), static_cast<i16>(vmag(static_cast<u16>(M3(dtopv)))));
        if (UW(M3(dballdist)) > 5200) nohome = true;
        else {
            SB(M3(closetowerflag), 1);
            if (UW(M3(dballdist)) > 4200) nohome = true;
            else {
                SB(M3(resetplanesflag), 0);
                SB(M3(closetowerflag), 0xFF);   // set the close to tower flag
                return;
            }
        }
    }
    SB(M3(resetplanesflag), 0xFF);   // reset all the planes
    SW(M3(nplaced), 0);
}
// ==========================================================================================
// 3.ASM getinplane: gets us into the plane at [si]
// ==========================================================================================
void Main3::getinplane(u16 si) {
    if (B(M3(ejectflag)) == 0) {
        m_eng.cecode = 77;
        return;
    }
    SB(M3(redamageflag), 0);   // do not use old damage
    if (W(si + 24) & 0x104) SW(M3(currpmx), 0);   // the plane is crashed: have it be dead
    else cleardamage();
    SW(M3(avgzvel), 0);
    SW(M3(avgdvelh), 0);
    SW(M3(avgdvell), 0);
    if (W(M3(obj5d) + 24) != 0) {   // is the object being displayed? funeral for what is left of it
        const bool smoking = W(M3(obj5d) + 40) == static_cast<i16>(M3(plnplyobjsm));
        funeral5d(smoking);
    }
    SW(M3(obj5dbits), static_cast<i16>(UW(si + 24) & 0x3106));   // the bits that matter
    for (int k = 0; k < 9; k++) SW(M3(ox) + 2 * k, W(si + 2 * k));   // move us to the orientation of the plane
    for (int k = 0; k < 3; k++) SW(M3(rtopv) + 2 * k, 0);
    for (int k = 0; k < 3; k++) SW(M3(ctopv) + 2 * k, 0);
    for (int k = 0; k < 3; k++) SW(M3(topv) + 2 * k, 0);
    SW(M3(pitdoth), 0);
    SW(M3(yawdoth), 0);
    SW(M3(thrdoth), 0);
    interiorview();   // put the instrument panel back on
    if (W(M3(currpmx)) != 0) {
        if (m_snd) m_snd->snareinit();       // turn the missile sound back on
        SB(M3(windflag), 0);                  // and the wind off
        damageinit();                         // re-init the damage and bomb displays
    }
    SB(M3(groundflag), 0xFF);
    SB(M3(flapflag), 0);          // flaps off
    SW(M3(rpm), 20);
    SB(M3(autoflag), 0);          // autopilot off
    SB(M3(nbombs), 3);
    SB(M3(freezeflag), 0xFF);     // keep the plane from rolling
    SB(M3(stepoutflag), 0);
    SB(M3(ejectflag), 0);         // get us off the ground
    SB(M3(standingflag), 0);
    SB(M3(pullstrflag), 0);       // allow the chute to work again
    SB(M3(numlock), B(M3(joyflag)));   // views come back on with the joystick
    // (the original waits here for the keypad 5 to be released when numlock is on)
    SB(M3(crshlndflag), static_cast<u8>(UW(M3(obj5dbits)) & 4));   // is the crash land bit set?
    if (B(M3(crshlndflag)) != 0) SW(M3(currpmx), 0);
    if (W(M3(currpmx)) == 0) {
        SB(M3(instflag), 0);   // turn off the instruments if the plane is dead
        const u8 c = B(M3(cmpltflag));
        SB(M3(cmpltflag), static_cast<u8>((c & 1) ? 0xFF : 0));   // if cmpltflag=1, make it -1
    }
    const i16 pl = static_cast<i16>(M3(plnplyobj));
    SW(M3(obj5d) + 40, pl);
    SW(M3(obj5d) + 42, pl);
}

// 3.ASM interiorview / fullsize / restoreview: the window set-up
void Main3::interiorview() {
    if (m_eng.cecode != 0) return;
    SW(M3(ptrgwinf), static_cast<i16>(M3(gwinf)));
    SW(M3(ptrgwint), static_cast<i16>(M3(gwint)));
    if (m_viewChanged) m_viewChanged(true);   // read in the image of the instrument panel
    SB(M3(instflag), 0xFF);                   // turn the instrument panel back on
}

void Main3::fullsize() {
    if (W(M3(ptrgwinf)) == static_cast<i16>(M3(gwinfs))) return;   // do nothing if already up
    SW(M3(ptrgwint), 0);   // turn off the rear window
    SB(M3(instflag), 0);   // turn off the instruments
    SW(M3(ptrgwinf), static_cast<i16>(M3(gwinfs)));   // turn on the full screen
    if (m_viewChanged) m_viewChanged(false);
}

void Main3::restoreview() {
    const i16 ax = W(M3(ptrgwinf));
    if (ax == static_cast<i16>(M3(gwinfs))) {
        fullsize();
        return;
    }
    if (ax == static_cast<i16>(M3(gwinf))) {
        interiorview();
        damageinit();
        redrawbuts();
        return;
    }
    m_eng.cecode = 29;
}

// 3.ASM drawtodxvec: turns the observer to look at the object at [si]
void Main3::drawtodxvec(u16 si, bool& tooFar) {
    tooFar = true;
    if (m_eng.rel3d(si)) return;
    const u16 cx = m_eng.rangeBits();
    if (cx >> 8) return;
    if (cx & 0xF0) return;   // too far
    for (int k = 0; k < 3; k++) {
        const u32 dd = static_cast<u32>(D(M3(dxvec) + 4 * k)) >> 6;
        SW(M3(xvec) + 2 * k, neg16(static_cast<i16>(dd & 0xFFFF)));
    }
    i16 ax = atn2(W(M3(xvec)), W(M3(xvec) + 2));   // atn2(delta-y / delta-x)
    ax = add16(ax, static_cast<i16>(32768));        // point in the opposite direction
    ax = sub16(ax, W(M3(orvec)));                   // the difference is what we work on
    const int cl = B(M3(haltflag)) != 0 ? 7 : 4;    // if being halted, slow down the rate
    ax = sar16(ax, cl);
    SW(M3(orvec), add16(W(M3(orvec)), wtadj(ax)));
    tooFar = false;
}

// 3.ASM donewmiss: a new mission was selected: move the player there
void Main3::donewmiss() {
    if (B(M3(newmissflag)) == 0) return;
    if (UW(M3(totaldamage)) >= 16) {
        SB(M3(newmissflag), 0);
        SW(M3(rmsgqueue), static_cast<i16>(M3(cantf2rmsg)));
        return;
    }
    m_world.flushtiles();
    m_eng.drawnear();
    for (int k = 0; k < 8; k++) SW(M3(ox) + 2 * k, W(M3(tempstart) + 2 * k));   // x, y, z, yaw
    m_eng.calcsin(static_cast<u16>(M3(orvec)));
    m_eng.calcmat(static_cast<u16>(lbl::orot));
    m_eng.calcmat(static_cast<u16>(M3(nzmat)));
    m_eng.ncalcmat(static_cast<u16>(lbl::norot));
    for (int k = 0; k < 9; k++) SW(M3(nnzmat) + 2 * k, W(lbl::norot + 2 * k));
    SW(M3(topv), 5000);
    SW(M3(topv) + 2, 0);
    SW(M3(topv) + 4, 0);
    m_eng.matvmul(static_cast<u16>(M3(topv)), static_cast<u16>(M3(nnzmat)), static_cast<u16>(M3(rtopv)));
    for (int k = 0; k < 3; k++) SW(M3(ctopv) + 2 * k, sar16(W(M3(topv) + 2 * k), 4));
    m_world.dotiles();   // read the objects back in
    SB(M3(newmissflag), 0);
    SB(M3(freezeflag), 0);
    SB(M3(standingflag), 0);
    SB(M3(groundflag), 0);
    SB(M3(homenowflag), 0);
    SB(M3(closetowerflag), 0);
    SB(M3(homeflag), 0);
    SB(M3(orbonflag), 0xFF);
    SW(M3(rpm), W(M3(currpmx)));   // advance the throttle to maximum
    const u32 num = static_cast<u32>(static_cast<u16>(W(M3(rpm)))) << 15;
    const u16 rpmx = static_cast<u16>(W(M3(rpmx)));
    const u16 q = rpmx ? static_cast<u16>(num / rpmx) : 0xFFFF;
    SW(M3(opres), static_cast<i16>((mul16(q, static_cast<u16>(W(M3(thrtmult)))) >> 16)));
}

// 3.ASM doxp: a portal asked for a transfer
void Main3::doxp() {
    const u8 xp = B(M3(xpflag));
    if (xp == 0) return;
    m_world.flushtiles();
    m_eng.drawnear();
    if (!(xp & 0x80)) {   // go to the start coordinates
        for (int k = 0; k < 6; k++) SW(M3(ox) + 2 * k, W(M3(startcoords) + 2 * k));
    } else {              // a random place
        SW(M3(ox) + 2, static_cast<i16>(m_rand.randr_w(8 * 32) - 4 * 32));
        SW(M3(ox) + 6, static_cast<i16>(m_rand.randr_w(8 * 32) - 4 * 32));
        SW(M3(ox) + 6 - 6, static_cast<i16>(m_rand.rand_w()));
        SW(M3(ox) + 4, static_cast<i16>(m_rand.rand_w()));
    }
    SW(M3(oz), static_cast<i16>(0x8000));   // start way up there
    SW(M3(oz) + 2, 3);
    m_world.dotiles();
    SB(M3(tripflag), static_cast<u8>(B(M3(tripflag)) & ~2));
    SB(M3(xpflag), 0);
}

// 3.ASM takedrugs: hallucinogens (the r, g and b keys shift the palette; 1 and 2 pick sky/ground)
void Main3::takedrugs() {
    SW(M3(haluclrate) + 2, wtadj(W(M3(haluclrate))));
    if (key(0x13)) SW(M3(haluclrs), add16(W(M3(haluclrs)), W(M3(haluclrate) + 2)));
    if (key(0x22)) SW(M3(haluclrs) + 2, add16(W(M3(haluclrs) + 2), W(M3(haluclrate) + 2)));
    if (key(0x30)) SW(M3(haluclrs) + 4, add16(W(M3(haluclrs) + 4), W(M3(haluclrate) + 2)));
    m_eng.distcolors();
    // (the sky and ground color pickers wait for key releases: not needed for play)
}

// MAT.ASM getang: the three angles (yaw, pitch, roll) out of a rotation matrix at [bx], into [si]
void Main3::getang(u16 bx, u16 si) {
    auto madd = [](i16 a, i16 b) { return asmx::madd(a, b); };
    const auto atn2 = [this](i16 x, i16 y) { return m_fineAngles ? engine::atn2Fine(x, y) : engine::atn2(x, y); };
    const i16 x = W(bx), y = W(bx + 6);
    const u8 yh = static_cast<u8>(static_cast<u16>(y) >> 8), xh = static_cast<u8>(static_cast<u16>(x) >> 8);
    const bool ysmal = yh == 0 || yh == 0xFF;
    const bool xsmal = xh == 0 || xh == 0xFF;
    i16 cya, sya;
    if (ysmal && xsmal) {   // x and y are too small for a good angle: straight up or down
        cya = 32767;
        sya = 0;
        i16 dx = 16384;     // put a 90 degree angle into dx
        if (!(W(bx + 12) < 0)) dx = neg16(dx);
        SW(si + 2, dx);
        SW(si, 0);          // reset the yaw angle
    } else {
        const i16 yaw = atn2(x, y);
        SW(si, yaw);
        sya = engine::ssin(yaw);
        const i16 h1 = hmul(sya, W(bx + 6));
        cya = engine::scos(yaw);
        const i16 h2 = hmul(cya, W(bx));
        const i16 s = madd(h1, h2);
        SW(si + 2, neg16(atn2(s, W(bx + 12))));
    }
    // getrol: the roll angle
    const i16 xtemp = madd(hmul(W(bx + 2), cya), hmul(W(bx + 8), sya));            // y comp of the y axis, unyawed
    const i16 ytemp = madd(neg16(hmul(W(bx + 2), sya)), hmul(W(bx + 8), cya));     // and the x comp
    const i16 pitch = W(si + 2);
    const i16 cpa = engine::scos(pitch);
    const i16 z1 = hmul(cpa, W(bx + 14));
    const i16 spa = engine::ssin(pitch);
    const i16 z2 = hmul(spa, xtemp);
    const i16 znew = madd(z2, z1);   // the new z coord height
    SW(si + 4, atn2(ytemp, znew));
}

// ==========================================================================================
// DEFOBJS.ASM initperms: the permanent objects of a training mission (and a mission without a
// scenario file): the runway, the tower, the hut, the walls of the world, the enemy sites ...
// ==========================================================================================
void Main3::initperms() {
    constexpr i16 W_ = d::m3_equ::w, ROADX = d::m3_equ::roadx, WALLSZ = 16383, LTW = 900;
    constexpr i16 TWSZ = 1000, TWSZL = 8000, DBSZ = 400, TETHT = 16384, TREEHT = 2000, FRMZL = 1000;
    const u32 kbuf = d::m3::kbuf, dxvec = d::m3::dxvec;
    using namespace d::defobjs;

    // places an object from template tmpl at the 9 words of kbuf, with this quality
    auto perm = [&](u32 tmpl, i16 quality) {
        SW(d::strtexp::initialqual, quality);
        u16 slot;
        m_world.strtperm(static_cast<u16>(kbuf), static_cast<u16>(tmpl), quality, slot);
    };
    auto kw = [&](int off, i16 v) { SW(kbuf + off, v); };
    // the 9 words of a record at once: x (lo, hi), y (lo, hi), z (lo, hi), then the three angles
    auto rec = [&](i16 x0, i16 x1, i16 y0, i16 y1, i16 z0, i16 z1, i16 a0, i16 a1, i16 a2) {
        const i16 v[9] = {x0, x1, y0, y1, z0, z1, a0, a1, a2};
        for (int k = 0; k < 9; k++) kw(2 * k, v[k]);
    };

    SW(d::defobjs::nplaced, add16(W(d::defobjs::nplaced), 1));   // put planes behind us
    for (int k = 0; k < 74; k++) SW(kbuf + 2 * k, 0);
    // the road: the pieces run off to the south of the start point
    kw(4, -W_ * 4);
    kw(6, -1);
    kw(8, 0);
    kw(10, 0);
    kw(0, -1);
    kw(2, ROADX);
    perm(objroadend, 0);
    addToDword(kbuf, -32768);   // advance the position
    for (int k = 0; k < 10; k++) {
        perm(roadpiece, 0);
        addToDword(kbuf, -32768);
    }
    perm(objroadend1, 0);
    SD(kbuf, static_cast<i32>(static_cast<u32>(D(M3(startcoords))) - 12000u));
    kw(4, 0);
    kw(6, 0);
    perm(roadpiece, 7);   // a non-striped piece

    if (B(M3(actionflag)) == 0) {
        auto randomSpot = [&](u32 at) {   // two random dwords, x and y
            i16 hi;
            i32 lo = grand(hi);
            SW(at, static_cast<i16>(static_cast<u32>(lo) & 0xFFFF));
            SW(at + 2, hi);
            lo = grand(hi);
            SW(at + 4, static_cast<i16>(static_cast<u32>(lo) & 0xFFFF));
            SW(at + 6, hi);
        };
        for (int k = 0; k < d::m3_equ::ngmo; k++) {   // the ground missile sites
            randomSpot(dxvec);
            SW(dxvec + 10, 0);
            SW(dxvec + 8, static_cast<i16>(d::m3_equ::xgmx2));
            SW(d::strtexp::initialqual, 0);
            u16 slot;
            m_world.strtperm(static_cast<u16>(dxvec), static_cast<u16>(gms), 0, slot);
        }
        // a random quality when the file says so
        auto quality = [&](i16 q) -> i16 { return q < 0 ? static_cast<i16>(m_rand.rand_w() & 7) : q; };
        auto raised = [&]() {   // the site sits a little off the ground
            randomSpot(dxvec);
            SW(dxvec + 8, 2 * DBSZ);
            SW(dxvec + 10, 0);
        };
        for (int n = W(d::m3::naas); n > 0; n--) {
            const i16 q = quality(W(M3(qualaa)));
            SW(d::strtexp::initialqual, q);
            raised();
            m_world.strtaa(static_cast<u16>(dxvec), q);
        }
        for (int k = 0; k < 6; k++) SW(dxvec + 2 * k, W(M3(ox) + 2 * k));
        SW(dxvec + 6, add16(W(dxvec + 6), -5));
        SW(dxvec + 2, add16(W(dxvec + 2), 2));
        SW(d::strtexp::maxnvechs, W(M3(nvest)));
        for (int n = W(M3(nvest)); n > 0; n--) {
            const i16 q = quality(W(M3(qualvest)));
            SW(d::strtexp::initialqual, q);
            raised();
            m_world.strtvest(static_cast<u16>(dxvec), q);
        }
        SW(d::strtexp::maxndballs, W(M3(ndbst)));
        for (int n = W(M3(ndbst)); n > 0; n--) {
            const i16 q = quality(W(M3(qualdbst)));
            SW(d::strtexp::initialqual, q);
            raised();
            m_world.strtdbst(static_cast<u16>(dxvec), q);
        }

        // the walls round the world
        rec(WALLSZ * 2, 0, 0, 0, WALLSZ, 0, 0, 0, 0);
        perm(objwallx, 4);
        kw(0, -WALLSZ * 2);
        kw(2, -1);
        kw(8, WALLSZ);
        perm(objwallx, 4);
        kw(0, 0);
        kw(2, 0);
        kw(8, 2 * WALLSZ);
        perm(objwallz, 4);

        // (saucey = 0) the bomber, the creature, the trees, the farm, the tetrahedra
        rec(0, -6, 32000, 1, 0, 2, 0, 0, 0);
        perm(objbmrst, 0);
        kw(0, 10000);
        kw(2, -6);
        kw(4, 22000);
        perm(objbmrst, 0);
        kw(4, 42000);
        perm(objbmrst, 0);

        rec(0, 3, 32000, 2, 0, 0, 0, 0, 0);
        perm(crtr, 0);

        for (int k = 0; k < 50; k++) {   // trees; the quality is whatever was left in ax (the segment)
            randomSpot(kbuf);
            kw(8, TREEHT);
            kw(10, 0);
            perm(objshell, 0);
        }
        rec(0, 0, -1, -2, FRMZL, 0, 0, 0, 0);
        perm(objbmr, 0);

        i16 tz = TETHT, tzHi = 0;
        for (int row = 0; row < 2; row++) {
            i16 ax = 0;
            for (int k = 0; k < 6; k++) {
                kw(0, 0);
                kw(2, ax);
                ax = add16(ax, 2);
                kw(4, 0);
                kw(6, 2);
                kw(8, tz);
                kw(10, tzHi);
                kw(12, 0);
                kw(14, 0);
                kw(16, 0);
                perm(objtet, 0);
            }
            const u32 sum = static_cast<u32>(static_cast<u16>(tz)) + static_cast<u32>(TETHT + TETHT);
            tz = static_cast<i16>(sum & 0xFFFF);
            tzHi = static_cast<i16>(tzHi + static_cast<i16>(sum >> 16));
        }
    }

    // the plane that got us here, the tower and the airport buildings, the survivors
    rec(8000, ROADX - 1, 8000, 0, W(M3(hmin)), 0, -8192, static_cast<i16>(d::m3_equ::epitchmin), 0);
    perm(obj5da, 0);
    rec(-1, ROADX, 16000, 0, LTW * 4, 0, 0, 0, 0);
    perm(objtw, 0);
    perm(objrdr, 0);
    rec(-1, ROADX, 22000, 0, 0, 0, 0, 0, 0);
    perm(objman, 0);
    rec(-1, ROADX - 2, 16000 - TWSZ * 2, 0, 0, 0, 0, 0, 0);
    perm(objhut, 0);
    rec(-1, ROADX - 2, 16000, 0, TWSZL * 2, 0, 0, 0, 0);
    perm(tower, 0);
    rec(0, 0, 0, 0, 0, 0, 0, 0, 0);
    perm(objsurv, 0);
    perm(objsurv, 0);
    rec(0, -32, 0, -32, 0, 0, 0, 0, 0);
    perm(objsurv, 0);
    rec(0, 0, 0, -32, 0, 0, 0, 0, 0);
    perm(objsurv, 0);
    rec(0, -32, 0, 0, 0, 0, 0, 0, 0);
    perm(objsurv, 0);
}

} // namespace corncob::game