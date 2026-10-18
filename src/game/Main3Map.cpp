// Main3Map.cpp - Map mode: MAP.ASM's maploop and its helpers, and DEFOBJS.ASM's changewallobjs
//
// The original's maploop is a loop of its own, entered from the main loop's map key. Here the loop
// is a state: frame() calls mapFrame() once per pass while m_mapActive, which is one trip round
// maplp. Only the part that is built without the mission builder (mapmbflag = 0) exists.

#include "Main3.hpp"
#include "DGroup.hpp"
#include "core/AsmMath.hpp"
#include "engine/AsmTables.hpp"

namespace corncob::game {

namespace d = corncob::dgroup;
namespace lbl = corncob::lbl;
using asmx::bndck;
using asmx::hmul;

#define M3(name) lbl::name

namespace {
inline i16 add16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) + static_cast<u16>(b))); }
inline i16 sub16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) - static_cast<u16>(b))); }
inline i16 neg16(i16 a) { return static_cast<i16>(static_cast<u16>(0u - static_cast<u16>(a))); }
inline u16 abs16(i16 a) { return static_cast<u16>(a < 0 ? neg16(a) : a); }
} // namespace

// ==========================================================================================
// DEFOBJS.ASM chgwloop / changewallobjs: the walls that breathe in and out (and the trip effects)
// ==========================================================================================
void Main3::chgwloop(u16& si, int cx, i16 dx) {
    for (; cx > 0; cx--) {
        const u16 bx = UW(si);
        SW(bx, W(bx) < 0 ? neg16(dx) : dx);   // keeps the sign of the size
        si = static_cast<u16>(si + 2);
    }
}

void Main3::changewallobjs() {
    constexpr i16 WALLSZ = 16383, WALLSZ2 = 8191, FSR = 4000;
    SW(M3(framecount), add16(W(M3(framecount)), static_cast<i16>(m_refSteps)));
    const u16 fc = UW(M3(framecount));

    // sizes of the walls of wallsz22tbl
    SW(M3(tripvalue) + 2, static_cast<i16>(static_cast<u16>(fc << 12)));
    i16 ax = engine::scos(W(M3(tripvalue) + 2));
    ax = add16(hmul(ax, WALLSZ2), WALLSZ2);
    if (ax == 0) ax = 1;
    if (ax < 0) ax = WALLSZ2 * 2;
    i16 dx = ax;
    u16 si = static_cast<u16>(M3(wallsz22tbl));
    chgwloop(si, W(M3(nwallsz22)), dx);
    dx = static_cast<i16>(dx << 1);   // make collision twice the size
    chgwloop(si, W(M3(nwallsz22b)), dx);

    // the two spheres, and the wavy sin used for tripping
    ax = engine::scos(static_cast<i16>(static_cast<u16>(fc * 3111u)));
    SW(M3(tripvalue), ax);
    const i16 cosv = ax;
    dx = hmul(ax, FSR / 12);
    SW(M3(fsph1sz), add16(dx, FSR / 6 + FSR / 12));
    SW(M3(fsph2sz), neg16(sub16(dx, FSR / 6 + FSR / 12)));

    ax = add16(hmul(cosv, WALLSZ2 / 2), WALLSZ2 / 2);
    if (ax == 0) ax = 1;
    if (ax < 0) ax = WALLSZ2;
    dx = ax;
    si = static_cast<u16>(M3(wallsz2tbl));
    chgwloop(si, W(M3(nwallsz2)), dx);
    dx = static_cast<i16>(dx << 1);
    chgwloop(si, W(M3(nwallsz2b)), dx);

    ax = engine::scos(static_cast<i16>(static_cast<u16>(fc * 5333u)));
    ax = add16(hmul(ax, WALLSZ / 2), WALLSZ / 2);
    if (ax == 0) ax = 1;
    if (ax < 0) ax = WALLSZ;
    dx = ax;
    si = static_cast<u16>(M3(wallsztbl));
    chgwloop(si, W(M3(nwallsz)), dx);
    dx = static_cast<i16>(dx << 1);
    chgwloop(si, W(M3(nwallszb)), dx);

    // horizon tripping: the colors drift
    if (B(M3(tripflag)) & 1) {
        for (int k = 0; k < 3; k++) {
            const i16 d = hmul(wtadj(W(M3(haluclrate) + 2 * k)), W(M3(palmul)));
            SW(M3(haluclrs) + 2 * k, add16(W(M3(haluclrs) + 2 * k), d));
        }
        m_eng.distcolors();
    }

    // distort the window a little
    i16 wx = 300, wy = 200;
    SW(M3(orbfreq), 128);
    if (B(M3(tripflag)) & 2) {
        const i16 amount = W(M3(tripamount));
        i16 dxs = hmul(engine::ssin(W(M3(tripvalue))), amount);
        wx = add16(300, sub16(dxs, static_cast<i16>(static_cast<u16>(amount) >> 1)));
        dxs = hmul(engine::ssin(W(M3(tripvalue) + 2)), amount);
        const i32 prod = static_cast<i32>(128) * dxs;
        const asmx::DivResult r = asmx::idiv32(prod, amount);
        SW(M3(orbfreq), r.q);   // make the orb frequency differ from 128
        wy = add16(200, sub16(dxs, static_cast<i16>(static_cast<u16>(amount) >> 1)));
    }
    const u16 win = UW(M3(ptrgwinf));
    SW(win + 12, wx);
    SW(win + 14, wy);
}

// ==========================================================================================
// MAP.ASM: the helpers
// ==========================================================================================
void Main3::restoreox() {
    const u32 src = B(M3(ejectflag)) != 0 ? M3(objman1) : M3(obj5d);
    for (int k = 0; k < 9; k++) SW(M3(ox) + 2 * k, W(src + 2 * k));
    recalcmats();
}

void Main3::setyah() {
    for (int k = 0; k < 9; k++) SW(M3(youarehere) + 2 * k, W(M3(ox) + 2 * k));
}

// the blinking "you are here" marker, only drawn from high enough up
void Main3::drawyah() {
    SB(M3(yahclr), (m_clock.oticks & 512) ? 15 : 0);
    const u16 bx = static_cast<u16>(UW(M3(z0)) >> 2);
    if (UW(M3(oz) + 2) < bx) return;
    m_eng.d3dobj(static_cast<u16>(M3(youarehere)), UW(M3(ptrgwinf)));
}

// joystick info with the small movements around the centre clipped away
void Main3::getjoy(i16& ax, i16& bx) {
    joytd(ax, bx);
    ax = sub16(ax, 0x200);
    bx = sub16(bx, 0x200);
    if (abs16(ax) <= 0x20) ax = 0;
    if (abs16(bx) <= 0x20) bx = 0;
}

// PgUp / PgDn zoom the map in and out (slower with s), z never drops below zero
void Main3::pgupanddown(u16 si) {
    i16 bx = 0;
    if (key(0x51) && UW(M3(oz) + 2) < 210) bx = 0x200 * 2;
    if (key(0x49)) bx = -0x200 * 2;
    if (bx != 0) {
        u32 v = static_cast<u32>(D(M3(oz))) >> 4;
        v += 40;   // don't let it go below 4
        i32 r = sas::muls_dw(static_cast<i32>(v), bx);
        r = sas::divs_dw(r, 0x200 * 2);
        r = dwtadj(r);
        if (key(0x1f)) r = r < 0 ? -1000 : 1000;   // the s key: slow
        addToDword(si + 8, r);
    }
    if (W(si + 10) < 0) {   // don't let z drop below zero
        SW(si + 10, 0);
        SW(si + 8, 0);
    }
}

// moves the coordinates at [si] with the stick, faster the higher up we are
void Main3::kinetic(u16 si) {
    i16 ax, bx;
    getjoy(ax, bx);
    ax = neg16(ax);
    bx = neg16(bx);
    const i16 first = bx, second = ax;   // after the xchg: the stick's y moves x, its x moves y
    u32 v = static_cast<u32>(D(M3(oz))) >> 4;
    i32 r = dwtadj(sas::divs_dw(sas::muls_dw(static_cast<i32>(v), first), 0x200 * 2));
    addToDword(si, r);
    r = dwtadj(sas::divs_dw(sas::muls_dw(static_cast<i32>(v), second), 0x200 * 2));
    addToDword(si + 4, r);
    pgupanddown(si);
}

// the keys of map mode: home resets the view, shift + stick turns it, the stick alone moves it
void Main3::mapkey() {
    if (key(0x47)) {   // home: back to the original coordinates
        const i32 oz = D(M3(oz));
        restoreox();
        SW(M3(orvec), 0);
        SW(M3(orvec) + 2, 16384);
        SW(M3(orvec) + 4, 0);
        recalcmats();
        SD(M3(oz), oz);
    }
    if (key(0x2a)) {   // shift
        i16 ax, bx;
        joytd(ax, bx);
        ax = sub16(ax, 0x200);
        bx = sub16(bx, 0x200);
        bumpangles(neg16(bx), neg16(ax));
        m_eng.calcsin(static_cast<u16>(M3(orvec)));
        m_eng.calcmat(static_cast<u16>(M3(orot)));
        m_eng.ncalcmat(static_cast<u16>(M3(norot)));
    } else {
        kinetic(static_cast<u16>(M3(ox)));
    }
}

// ==========================================================================================
// MAP.ASM maploop
// ==========================================================================================
void Main3::mapRequest() {
    // 3.ASM 7259-7281: the map key. The "read the message first" wait of the original is not needed
    // (the messages are not shown); a training mission is told it has no map.
    if (!scnIs(0x32)) return;
    clearScn();
    if (B(M3(rmsgflag)) != 0 && B(M3(readfileflag)) == 0) return;
    // 7282-7302: from the cockpit go full screen with the sound off, then restore it afterwards
    m_mapCockpit = static_cast<u16>(W(UW(M3(ptrgwinf)) + 6)) <= static_cast<u16>(d::m3_equ::gfymax);
    if (m_mapCockpit) {
        fullsize();
        SW(M3(orotptr), static_cast<i16>(M3(orot)));
        SW(M3(norotptr), static_cast<i16>(M3(norot)));
        if (m_snd) m_snd->soundoff();
    }
    mapStart();
}

void Main3::mapStart() {
    const u8 desire = B(M3(desireflag));
    SB(M3(desireflag), 0);
    SB(M3(olddesireflag), desire);
    SW(M3(framecount), 0);
    changewallobjs();
    SB(M3(clrhq), 0);

    const u32 dst = B(M3(ejectflag)) != 0 ? M3(objman1) : M3(obj5d);
    for (int k = 0; k < 9; k++) SW(dst + 2 * k, W(M3(ox) + 2 * k));   // remember where we are
    setyah();
    SD(M3(oz), static_cast<i32>(210u << 16));
    SW(M3(z0), 35);
    SW(M3(orvec) + 2, 16384);   // looking straight down
    SW(M3(orvec) + 4, 0);
    SW(M3(orvec), 0);
    m_eng.calcsin(static_cast<u16>(M3(orvec)));
    m_eng.calcmat(static_cast<u16>(M3(orot)));
    m_eng.ncalcmat(static_cast<u16>(M3(norot)));
    m_mapActive = true;
}

// one trip round maplp; the loop is left on escape, return, space or the map key
void Main3::mapFrame() {
    SB(M3(survclr), (m_clock.oticks & 1024) ? 0 : 0x0E);
    m_clock.frmticks = 0;
    const u16 win = UW(M3(ptrgwinf));
    m_eng.epage();
    SW(M3(viewkludge), W(M3(orvec) + 2));
    m_eng.modhr();
    SB(M3(hrflg), 0xFF);   // turn on fills
    m_eng.d3dobj(static_cast<u16>(M3(hrobj)), win);
    SB(M3(hrflg), 0);
    m_world.drawmap();
    m_eng.d3dobj(static_cast<u16>(B(M3(ejectflag)) != 0 ? M3(objman1) : M3(obj5d)), win);   // ourself, or our plane
    drawyah();
    drawgs();
    m_eng.flipage();
    mapkey();

    if (m_eng.cecode == 0) {
        const u8 code = static_cast<u8>(scncode() ^ 0x80);
        if (code != 1 && code != 0x1C && code != 0x32 && code != 0x39) return;
    }
    // mapexit
    clearScn();
    SW(M3(z0), 0);
    restoreox();
    SB(M3(desireflag), B(M3(olddesireflag)));
    m_mapActive = false;
    if (m_mapCockpit) {   // back into the cockpit
        if (m_snd) m_snd->soundon();
        SB(M3(redamageflag), 0xFF);   // use the old damage
        interiorview();
        damageinit();
        redrawbuts();
    }
    m_clock.frmticks = 0;
    m_clock.dticks = 0;
    m_clock.tdticks = 0;
}

} // namespace corncob::game
