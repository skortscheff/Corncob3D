// Main3Frame.cpp - Port of the 3.ASM main loop: the order of a frame and the stages around the flight model
//
// Line numbers in comments are 3.ASM's. frame() runs the stages in the original's order; the flow
// jumps of the ASM (freeze4, oxchanged, done4 ...) became early returns and flags.

#include "Main3.hpp"
#include "DGroup.hpp"
#include "core/AsmMath.hpp"
#include "engine/AsmTables.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace corncob::game {

namespace d = corncob::dgroup;
using asmx::bndck;
using asmx::hmul;
using asmx::idiv32;
using asmx::imul16;
using asmx::sf16;
using engine::atn2;
namespace lbl = corncob::lbl;

#define M3(name) lbl::name

namespace {

inline i16 add16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) + static_cast<u16>(b))); }
inline i16 sub16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) - static_cast<u16>(b))); }
inline i16 neg16(i16 a) { return static_cast<i16>(static_cast<u16>(0u - static_cast<u16>(a))); }
inline i16 abs16(i16 a) { return a < 0 ? neg16(a) : a; }
inline i16 sar16(i16 a, int n) { return static_cast<i16>(a >> n); }
inline i32 add32(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b)); }
inline i32 sub32(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) - static_cast<u32>(b)); }
inline i32 mk32(u16 lo, u16 hi) { return static_cast<i32>(static_cast<u32>(lo) | (static_cast<u32>(hi) << 16)); }
inline i16 lo16(i32 v) { return static_cast<i16>(static_cast<u32>(v) & 0xFFFF); }
inline i16 hi16(i32 v) { return static_cast<i16>(static_cast<u32>(v) >> 16); }
inline u32 mul16(u16 a, u16 b) { return static_cast<u32>(a) * b; }

constexpr int EHMIN = 720;

} // namespace

// ==========================================================================================
// start-up (3.ASM main, tgmode ... ship3): what the loop expects when it starts
// ==========================================================================================
void Main3::startUp(bool readFile) {
    SB(M3(readfileflag), readFile ? 0xFF : 0);
    SB(M3(joyflag), 0);                  // keyboard only until Phase 6
    SB(M3(numlock), B(M3(joyflag)));     // views off with the keyboard
    SB(M3(ejectflag), 0);
    // the start position: ox, oy, oz and the three angles
    for (int k = 0; k < 9; k++) SW(M3(ox) + 2 * k, W(M3(startcoords) + 2 * k));
    SB(M3(onrunwayflag), 0xFF);          // start out on the runway
    SB(M3(closetowerflag), 0xFF);        // start out close to the tower
    for (int k = 0; k < 9; k++) SW(M3(oxeff) + 2 * k, W(M3(ox) + 2 * k));
    // obj5d's motion constants, the ground detail points
    {
        const u16 si = static_cast<u16>(M3(obj5d));
        SW(M3(objdamp), W(si + 62));
        SW(M3(objdisp), W(si + 60));
        SW(M3(tckgrav), W(si + 64));
    }
    std::vector<uint8_t>& gs = m_eng.gscum();
    gs[0] = static_cast<uint8_t>(engine::F3dVec::GNPTS);
    gs[1] = 0;
    // the second record of each array starts 4000 along y
    for (u32 base : {engine::F3dVec::GPTS, engine::F3dVec::FGPTS, engine::F3dVec::SGPTS}) {
        gs[base + 14 + 4] = 4000 & 0xFF;
        gs[base + 14 + 5] = 4000 >> 8;
    }
    for (int k = 1; k <= engine::F3dVec::GNPTS; k++) grndpt(engine::F3dVec::GPTS + 14u * k);
    for (int k = 1; k <= engine::F3dVec::GNPTS; k++) {
        fgrndpt(engine::F3dVec::FGPTS + 14u * k);
        gs[engine::F3dVec::FGPTS + 14u * k + 12] = 1;
    }
    for (int k = 1; k <= engine::F3dVec::GNPTS; k++) {
        sgrndpt(engine::F3dVec::SGPTS + 14u * k);
        gs[engine::F3dVec::SGPTS + 14u * k + 12] = 1;
    }
    // rotation matrices
    m_eng.calcsin(static_cast<u16>(M3(rvec)));
    m_eng.calcmat(static_cast<u16>(M3(rmat)));
    recalcmats();
    for (int k = 0; k < 9; k++) SW(M3(nnzmat) + 2 * k, W(lbl::norot + 2 * k));
    m_eng.calcsin(static_cast<u16>(M3(orvec)));
    m_eng.calcmat(static_cast<u16>(M3(nzmat)));
    m_eng.ncalcmat(static_cast<u16>(M3(zmat)));

    if (readFile && B(M3(actionflag)) == 0) {
        // (3.ASM icaresreaduniv) start on foot next to the airstrip
        for (int k = 0; k < 6; k++) SW(M3(ox) + 2 * k, W(M3(startcoords) + 2 * k));
        SD(M3(oy), sub32(D(M3(oy)), 3000));
        SB(M3(ejectflag), 0xFF);
        SB(M3(standingflag), 0xFF);
        SW(M3(obj5d) + 24, 0);   // make sure that image is off
        fullsize();
        SB(M3(numlock), 0);      // default is views off
    } else {
        SB(M3(newguyflag), 0);
        initperms();   // the runway, the tower and the rest of the permanent objects
        SD(M3(ox) + 4, sub32(D(M3(ox) + 4), 8000));
        SW(M3(ox) + 12, 0);
        recalcmats();
    }
    // is the sky dark? then the ground detail is yellow, and the sun a moon
    {
        const u16 sky = static_cast<u16>(M3(skycolors));
        if (B(sky) + B(sky + 1) + B(sky + 2) <= 0x30) {
            SB(M3(gndcolor), 0x0E);
            SB(d::defobjs::sunclr, 15);
        }
    }
    SW(M3(vboff), 0x7e00);
    SW(M3(n), 7);
    SB(M3(redamageflag), 0);   // do not use old damage
    {   // draw the needles once into both pages, so there is something to erase
        const u8 inst = B(M3(instflag));
        SB(M3(instflag), 0xFF);
        drawndles();
        m_eng.flipage();
        drawndles();
        m_eng.flipage();
        SB(M3(instflag), inst);
    }
    damageinit();
    SB(SCNCODE, 0);
    SB(M3(scnflg), 0);
    m_clock.frmticks = 0;
}

// ==========================================================================================
// One pass of the main loop
// ==========================================================================================
bool Main3::frame() {
    if (m_done) return false;

    timing();
    if (m_mapActive) {   // maploop: the map takes the place of the whole frame
        mapFrame();
        return true;
    }
    if (pauseAndHelp()) return true;   // paused: nothing moves, nothing is drawn

    // 7259-7303: the map key
    mapRequest();
    if (m_mapActive) return true;

    m_clock.frmticks = 0;
    m_clock.dticks = 0;
    m_clock.tdticks = 0;
    SW(M3(frmnum), add16(W(M3(frmnum)), 1));
    if (UW(M3(frmnum)) >= 16384) SW(M3(frmnum), 128);   // don't let the frame number roll over
    m_eng.epage();

    homeArrow();
    erasndles();
    viewMatrices();

    drawWorld();
    engineThermo();
    freezeAndAngles();

    i16 roll = 0, pitch = 0;
    sticks(roll, pitch);
    if (B(M3(freezeflag)) == 0) {   // stillcounts: a frozen plane does not fly
        flightDynamics(roll, pitch);
        forcesAndVelocity();
        groundAndCrash();
    }
    movement();
    everyFrame();
    matricesAfterMove();
    dodvelavg();
    altitudeLights();
    if (highAltitude()) return finishMission();
    if (key(0x23)) takedrugs();          // 'h': hallucinogens
    getBackIntoPlane();
    viewKeys();
    propAndWeapons();
    throttleKeys();
    if (B(M3(crshflg)) != 0) {
        crash();
        if (B(M3(invincible)) == 0) return finishMission();
    }
    miscKeys();
    return exitCheck();
}

// ==========================================================================================
// 7520-7976: the picture of the world
// ==========================================================================================
void Main3::drawWorld() {
    const u16 ptrgwinf = UW(M3(ptrgwinf));
    m_clock.dticks = 0;
    m_eng.modhr();
    SB(M3(hrflg), 0xFF);   // turn on fills
    m_eng.d3dobj(static_cast<u16>(M3(hrobj)), ptrgwinf);
    SB(M3(hrflg), 0);

    if (B(M3(bhighflag)) == 0) {   // way up high: no more objects
        if (B(M3(gdflg)) != 0) drawGroundDots();

        if (!(UW(M3(obj5dbits)) & 0x2000)) {   // no sun when in the bomber
            const u16 xvec = static_cast<u16>(M3(xvec));
            SW(xvec, 0);
            const i16 sunangle = W(M3(sunangle));
            SW(xvec + 2, hmul(neg16(engine::ssin(sunangle)), 1000));
            SW(xvec + 4, hmul(engine::scos(sunangle), 1000));
            m_eng.drawrdirect(static_cast<u16>(M3(objsun)), ptrgwinf);
        }

        m_world.drawmemseg();
        if (m_eng.cecode != 0) return;   // emergency disengage
        orbAndSaucerSound();
        klaSound();
        initnumbers();
        bulmov();
        expmov();                // STRTEXP expmov: primes the new timed objects, then moves them
        SB(M3(onrunwayflag), 0);   // this flag must be recalculated every frame
        SD(M3(zground), 0);        // default ground height is zero
        SD(M3(myzground), 0);
        m_eng.drawnear();
        doxp();                    // a portal asked for a transfer
        donewmiss();               // a new mission was selected
        m_world.copytable();
        m_world.dotiles();
    }

    // 7824-7857: the plane seen from outside while remote controlling it
    if (B(M3(ejectflag)) != 0 && B(M3(remoteflag)) != 0) {
        const u16 saved = UW(M3(ptrgwinf));
        const u16 tail = static_cast<u16>(M3(gwinl));
        SW(M3(ptrgwinf), static_cast<i16>(tail));
        m_eng.eraseWin(tail);
        SW(M3(xvec1), 7000);
        SW(M3(xvec1) + 2, 0);
        SW(M3(xvec1) + 4, 0);
        m_eng.matvmul(static_cast<u16>(M3(xvec1)), static_cast<u16>(M3(norot)), static_cast<u16>(M3(xvec)));
        m_eng.drawrdirect(static_cast<u16>(M3(obj5d)), tail);
        SW(M3(ptrgwinf), static_cast<i16>(saved));
        bool tooFar;
        drawtodxvec(static_cast<u16>(M3(obj5d)), tooFar);   // dxvec now points away from the plane
    }

    // 7858-7895: halted by a death ball: the screen fades out while looking at it
    if (B(M3(ejectflag)) != 0 && B(M3(haltflag)) != 0) {
        SB(M3(remoteflag), 0);
        bool tooFar;
        drawtodxvec(static_cast<u16>(M3(haltox)), tooFar);
        SW(d::m3::stepsz, wtadj(500));
        goodstep(W(d::m3::stepsz));   // step without shaking the screen
        if (static_cast<int8_t>(B(M3(haltflag))) >= 0) {   // only a positive haltflag can kill
            i16 rate = wtadj(W(M3(faderate)));
            if (rate == 0) rate = 1;
            u16 fade = static_cast<u16>(B(M3(fadecolorl)));
            i32 v = static_cast<i32>(fade) - static_cast<u16>(rate);
            if (v < 0) v = 0;
            SB(M3(fadecolorl), static_cast<u8>(v & 0xFF));
            if (!(B(M3(tripflag1)) & 1)) m_eng.setcolors(static_cast<u8>((v >> 8) & 0xFF));
        }
    }

    // the prop, seen from behind
    if (B(M3(ejectflag)) == 0 && W(M3(rpm)) <= 50 && W(M3(currpmx)) != 0)
        m_eng.d3dobj(static_cast<u16>(M3(objprop)), ptrgwinf);
    if (B(M3(ejectflag)) == 0 || !(key(0x4C) || key(0x52))) drawgs();   // the gunsight
    // the control tick marks and the needles, only with the cockpit window up
    if (static_cast<u16>(W(ptrgwinf + 6)) <= static_cast<u16>(d::m3_equ::gfymax)) {
        m_inst.clearticks();
        m_inst.drawairtick(W(M3(xang)));
        m_inst.drawelvtick(W(M3(yang)));
        m_inst.drawrdrtick(W(M3(rdrang)));
        drawndles();
    }
    m_eng.flipage();
}

// 7608-7606: ground detail points and stars
void Main3::drawGroundDots() {
    for (int k = 0; k < GNPTS; k++) {
        const u32 pt = engine::F3dVec::GPTS + 14u * k;
        const bool carry = m_eng.g3dobj(pt, false);
        if (carry) grndpt(pt);
        if (m_rand.rand_w() <= static_cast<u16>(65535 / GNPTS / 9)) grndpt(pt);   // lucky enough to try a new point
    }
    if (B(M3(starsflag)) != 0) {   // now the stars, drawn white
        const u8 old = B(M3(gndcolor));
        SB(M3(gndcolor), 15);
        for (int k = 0; k < GNPTS; k++) m_eng.g3dobj(engine::F3dVec::SGPTS + 14u * k, true);
        SB(M3(gndcolor), old);
    }
}

// ==========================================================================================
// 3.ASM grand / drandr / grndpt / sgrndpt / fgrndpt: random ground detail points
// ==========================================================================================
i32 Main3::grand(i16& hi) {
    const u16 range = 6;   // gsize
    const i16 dx = static_cast<i16>(m_rand.randr_w(range));
    const u16 ax = m_rand.rand_w();
    hi = static_cast<i16>(dx - range / 2);
    return static_cast<i32>(static_cast<u32>(ax));
}

i32 Main3::drandr(i32 range) {
    const u16 rlo = static_cast<u16>(range & 0xFFFF), rhi = static_cast<u16>(static_cast<u32>(range) >> 16);
    const u16 rnd = m_rand.rand_w();
    const u32 p1 = mul16(rlo, rnd);
    const u32 p2 = mul16(rhi, rnd) + (p1 >> 16);
    const i32 half = range >> 1;
    return sub32(static_cast<i32>(p2), half);
}

namespace {
void putDw(std::vector<uint8_t>& v, u32 off, i32 x) {
    const u32 u = static_cast<u32>(x);
    for (int k = 0; k < 4; k++) v[off + k] = static_cast<uint8_t>(u >> (8 * k));
}
} // namespace

void Main3::grndpt(u32 pt) {
    if (hi16(D(M3(oz))) < 0) return;   // abort if underground
    std::vector<uint8_t>& gs = m_eng.gscum();
    const i32 spread = static_cast<i32>(static_cast<u32>(D(M3(oz))) << 6);
    putDw(gs, pt, add32(drandr(spread), D(M3(ox))));          // random 0 to oz around the plane
    putDw(gs, pt + 4, add32(drandr(spread), D(M3(oy))));
    putDw(gs, pt + 8, 0);
    gs[pt + 12] = 0;
    gs[pt + 13] = 0;
}

void Main3::fgrndpt(u32 pt) {
    std::vector<uint8_t>& gs = m_eng.gscum();
    i16 hi;
    i32 lo = grand(hi);
    putDw(gs, pt, static_cast<i32>(static_cast<u32>(lo) | (static_cast<u32>(static_cast<u16>(hi)) << 16)));
    lo = grand(hi);
    putDw(gs, pt + 4, static_cast<i32>(static_cast<u32>(lo) | (static_cast<u32>(static_cast<u16>(hi)) << 16)));
    putDw(gs, pt + 8, 0);
    gs[pt + 12] = 0;
    gs[pt + 13] = 0;
}

void Main3::sgrndpt(u32 pt) {
    std::vector<uint8_t>& gs = m_eng.gscum();
    const u16 ssize = 16384;
    for (int k = 0; k < 2; k++) {
        const i16 dx = static_cast<i16>(m_rand.randr_w(ssize));
        const u16 ax = m_rand.rand_w();
        const i16 hi = static_cast<i16>(dx - ssize / 2);
        putDw(gs, pt + 4 * k, static_cast<i32>(static_cast<u32>(ax) | (static_cast<u32>(static_cast<u16>(hi)) << 16)));
    }
    putDw(gs, pt + 8, static_cast<i32>(static_cast<u32>(ssize / 8) << 16));
    gs[pt + 12] = 0;
    gs[pt + 13] = 0;
}

// ==========================================================================================
// 7978-7997: engine temperature
// ==========================================================================================
void Main3::engineThermo() {
    const u16 rpmx = static_cast<u16>(W(M3(rpmx)));
    const u32 num = static_cast<u32>(static_cast<u16>(W(M3(rpm)))) << 15;
    u16 q = rpmx ? static_cast<u16>(num / rpmx) : 0xFFFF;
    const u32 prod = mul16(q, static_cast<u16>(W(M3(thrtmult))));
    i16 ax = sub16(static_cast<i16>(prod >> 16), W(M3(opres)));
    SW(M3(opres), add16(W(M3(opres)), static_cast<i16>(ax / 40)));
    engineSound();
}

// ==========================================================================================
// 8074-8157: frozen/crash-landed plane handling and the angles the physics starts from
// ==========================================================================================
void Main3::freezeAndAngles() {
    if (B(M3(freezeflag)) != 0) {   // the help button flashes: press + to get going
        if (B(M3(helpflashflag)) != 0) drawbutton((m_clock.oticks & (1024 + 512)) ? 0x0107 : 0x0701, 12);
        return;
    }
    // engine temperature
    {
        const u16 rpmx = static_cast<u16>(W(M3(rpmx)));
        const u32 num = static_cast<u32>(static_cast<u16>(W(M3(rpm)))) << 15;
        const u16 q = rpmx ? static_cast<u16>(num / rpmx) : 0xFFFF;
        const i16 ax = sub16(static_cast<i16>(q), W(M3(etemp)));
        SW(M3(etemp), add16(W(M3(etemp)), static_cast<i16>(ax / 180)));
    }
    if (B(M3(crshlndflag)) != 0) {   // crash landed: a flat spin on the ground
        if (B(M3(ejectflag)) == 0) {
            SW(M3(orvec) + 2, 0);
            SW(M3(orvec) + 4, 0);   // make the plane right side up
            SW(M3(orvec), add16(W(M3(orvec)), W(M3(crshvel))));
            SD(M3(oz), add32(static_cast<i32>(static_cast<u16>(W(M3(hmin)))), D(M3(zground))));
        } else {
            SW(M3(obj5d) + 14, 0);
            SW(M3(obj5d) + 16, 0);
            SW(M3(obj5d) + 2, 0);
            SW(M3(obj5d) + 12, add16(W(M3(obj5d) + 12), W(M3(crshvel))));
            SW(M3(obj5d) + 8, W(M3(hmin)));
        }
    }
    u16 bx = static_cast<u16>(M3(orvec));
    if (B(M3(ejectflag)) != 0) {
        m_eng.calcsin(bx);
        m_eng.calcmat(static_cast<u16>(lbl::orot));
        m_eng.ncalcmat(static_cast<u16>(lbl::norot));
        bx = static_cast<u16>(M3(obj5d) + 12);
    }
    SW(M3(oldang), W(bx + 2));
    SW(M3(oldyaw), W(bx));
    SW(M3(oldpit), W(bx + 2));
    SW(M3(oldrol), W(bx + 4));
    m_eng.calcsin(bx);
    m_eng.ncalcmat(static_cast<u16>(M3(zmat)));
}

// ==========================================================================================
// 9143-9248: crash landings, being too low, the landing gear and the crash decisions
// ==========================================================================================
void Main3::groundAndCrash() {
    if (B(M3(crshlndflag)) != 0) {   // crash landed: the plane skids to a stop
        SW(M3(brdrcnt), add16(W(M3(brdrcnt)), 2));   // keep it shaking
        SW(M3(rtopv) + 4, 0);                         // clear the z velocity
        i16 x = static_cast<i16>(hi16(imul16(W(M3(rtopv)), 30000)) << 1);   // attenuate x and y velocity
        SW(M3(rtopv), x);
        i16 y = static_cast<i16>(hi16(imul16(W(M3(rtopv) + 2), 30000)) << 1);
        SW(M3(rtopv) + 2, y);
        const i16 sum = add16(abs16(y), abs16(x));
        SW(M3(crshvel), sum);
        if (!(static_cast<u16>(sum) > 200)) {
            adlwrt(0xBD, 0x20);                  // turn off the scrape noise
            SW(M3(brdrcnt), 0);                  // stop the plane from shaking
            SB(M3(freezeflag), 0xFF);            // stop motion of the plane
            SW(M3(currpmx), 0);                  // kill the throttle
            if (B(M3(ejectflag)) != 0) funeral5d(true);   // mark the plane as destroyed
        }
    }

    // is the plane low? (oz - zground, with hlow and hmin)
    i32 h = sub32(D(M3(ozeff)), D(M3(zground)));
    bool instacrash = false, planeishigh = false, runFindHeight = false;
    if (hi16(h) < 0) {
        SB(M3(crshflg), 4);
        instacrash = true;
    } else {
        h = sub32(h, static_cast<i32>(static_cast<u16>(W(M3(hlow)))));
        h = sub32(h, static_cast<i32>(static_cast<u16>(W(M3(hmin)))));
        if (hi16(h) >= 0) planeishigh = true;
        else runFindHeight = true;
    }
    if (planeishigh) {
        if (B(M3(homeflag)) == 0) SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) & ~4));
        SB(M3(groundflag), 0);   // don't say we are on the ground
    } else {
        bool o5dcrash = false;
        if (runFindHeight) instacrash = findheight();   // carry = a crash
        if (instacrash) {
            if (B(M3(ejectflag)) != 0) o5dcrash = true;
            else SB(M3(crshflg), B(M3(crshflg)) | 1);   // crash us bad
        }
        if (!o5dcrash) {   // planefine
            const bool crashed = dolandinggear();
            if (crashed) {
                if (B(M3(ejectflag)) == 0) SB(M3(crshflg), 2);   // yescrash
                else o5dcrash = true;
            }
        }
        if (o5dcrash) {
            SB(M3(crshflg), 0);
            if (!(UW(M3(obj5dbits)) & 0x100)) {   // the plane is destroyed: we definitely lost it
                SW(M3(nplaneslost), add16(W(M3(nplaneslost)), 1));
                const u8 al = static_cast<u8>((B(M3(homeflag)) << 1) & 2);
                SW(M3(obj5dbits), static_cast<i16>(UW(M3(obj5dbits)) | 0x100 | al));
            }
            odest(static_cast<u16>(M3(obj5d)), 0, true);   // destroy the plane
            SW(M3(currpmx), 0);
        }
    }

    // planeabove:
    qcrashland();
    qhomebase();
    for (int cx = 4; cx; cx--) flashdamage(cx - 1);
    if (UW(M3(totaldamage)) >= 50) flasheject();
}

// ==========================================================================================
// 9249-9430: moving: the plane (or the man on foot, or falling with his chute)
// ==========================================================================================
void Main3::movement() {
    if (B(M3(ejectflag)) == 0) {
        if (B(M3(freezeflag)) == 0) moveme();   // apply velocity rtopv to ox, oy, oz
        qhomebase();                             // do home base stuff here if frozen
    } else {
        ejectedMovement();
    }
    // oxchanged:
    if (B(M3(pullstrflag)) != 0 && static_cast<int8_t>(B(M3(pullstrflag))) > 0) {   // has the string been pulled?
        SW(M3(swingcount), static_cast<i16>(d::m3_equ::swingmx + 8));   // swing in the breeze
        SB(M3(pullstrflag), 0xFF);                                      // then free up the angles
    }
    if (B(M3(crshlndflag)) != 0) {
        u16 bit = 0x40;   // the default is a lousy crash landing bit
        if (B(M3(homeflag)) != 0) bit = 0x200;   // a nice crash landing
        SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) | bit));
    }
}

void Main3::ejectedMovement() {
    // jumping
    bool toCripcrap = false;
    bool skipJumpCheck = false;
    if (B(M3(standingflag)) != 0 && key(0x24)) {   // 'j' charges up a jump
        SB(M3(jumpflag), 0xFF);
        jumpadd();
        skipJumpCheck = true;   // nojumpatall
    }
    if (!skipJumpCheck) {
        if (B(M3(jumpflag)) != 0) {   // were we charging a jump?
            SB(M3(jumpflag), 0);
            jumpadd();
            i16 ax = W(M3(jumpvalue));
            SW(M3(jumpvalue), 0);
            if (ax != 0) {
                ax = static_cast<i16>(static_cast<u16>(ax) << 2);   // jump a little higher
                SW(M3(otopv) + 4, ax);
                SB(M3(pullstrflag), 0);
                SW(M3(chutedamp), 31000);
                SW(M3(termvel), static_cast<i16>(d::m3_equ::freetermvel));
                toCripcrap = true;
            }
        }
    }
    bool onGround = false;
    if (!toCripcrap) {
        if (key(0x12)) {   // 'e': drop through the surface
            SD(M3(myzground), 0);
            toCripcrap = true;
        } else {
            i32 h = sub32(D(M3(oz)), EHMIN);
            h = sub32(h, D(M3(myzground)));
            if (h < 0 || h == 0) onGround = true;   // on the ground: stop
            else toCripcrap = true;
        }
    }
    if (toCripcrap) {
        SB(M3(standingflag), 0);
        for (int k = 0; k < 3; k++) {   // all velocities divided by 16 before applying
            const i16 ax = sar16(W(M3(otopv) + 2 * k), 4);
            addToDword(M3(ox) + 4 * k, dwtadj(static_cast<i32>(ax)));
        }
        i16 ax = static_cast<i16>(d::m3_equ::freefallgrav);
        if (B(M3(pullstrflag)) != 0) {   // has the string been pulled?
            SW(M3(chutedamp), 30000);
            SW(M3(termvel), static_cast<i16>(d::m3_equ::chutetermvel));
        }
        ax = wtadj(ax);   // scale with the machine speed
        SW(M3(otopv) + 4, sub16(W(M3(otopv) + 4), ax));
        if (B(M3(standingflag)) == 0) {
            dampchute();
            i32 h = sub32(D(M3(oz)), D(M3(myzground)));
            h = sub32(h, static_cast<i32>(static_cast<u16>(W(M3(hmin)))));
            if (hi16(h) < 0) {   // below the ground (plus hmin)
                const u16 fall = static_cast<u16>(neg16(W(M3(otopv) + 4)));
                bool hard = true;
                if (fall < 2 * d::m3_equ::chutetermvel) hard = false;
                else if (B(M3(pullstrflag)) == 0 && fall < d::m3_equ::freetermvel * 2 / 3) hard = false;
                if (hard && B(M3(invincible)) != 0) hard = false;
                if (hard) SB(M3(crshflg), 3);   // uh oh, we died
                else onGround = true;           // notoohard
            }
        } else {
            onGround = true;
        }
    }
    if (onGround) {   // notoohard: standing on the surface
        SW(M3(swingcount), 0);
        SW(M3(otopv) + 4, 0);
        SD(M3(oz), add32(static_cast<i32>(static_cast<u16>(W(M3(hmin)))), D(M3(myzground))));
        SB(M3(standingflag), 0xFF);
    }
    // the plane keeps going while we are out of it
    if (W(M3(obj5d) + 10) < 0) return;   // if the plane already crashed, don't move it
    if (B(M3(freezeflag)) != 0 || W(M3(currpmx)) == 0) return;
    for (int k = 0; k < 3; k++) {
        const i16 ax = sar16(W(M3(rtopv) + 2 * k), 4);
        addToDword(M3(obj5d) + 4 * k, dwtadj(static_cast<i32>(ax)));
    }
    if (W(M3(obj5d) + 10) < 0) odest(static_cast<u16>(M3(obj5d)), 0, true);
}

// ==========================================================================================
// 9447-9560: animated colors and the spheres of explosions, then the view matrices
// ==========================================================================================
void Main3::everyFrame() {
    u8 al = 0;
    if (B(M3(homeflag)) != 0 && (m_clock.oticks & 1024)) al = 15;   // the home base blinks
    SB(M3(clrtwr), al);
    // exchange the red and yellow flame colors
    {
        u8 a = B(M3(clrf1)), b = B(M3(clrf2));
        SB(M3(clrf1), b);
        SB(M3(clrf2), a);
        a = B(M3(clrd1));
        b = B(M3(clrd2));
        SB(M3(clrd1), b);
        SB(M3(clrd2), a);
        a = B(M3(clrd3));
        b = B(M3(clrd4));
        SB(M3(clrd3), b);
        SB(M3(clrd4), a);
    }
    SB(M3(clrhq), static_cast<u8>(m_rand.randr_w(15)));   // give the headquarters a random color

    // the explosion spheres: every second word of each 18 byte record gets a random offset
    auto jitter = [&](u32 src, u32 dst, int records, u16 range, int step, bool plus2l) {
        u32 si = src + 2, di = dst + 2;
        u16 bx = range;
        for (int rec = 0; rec < records; rec++) {
            for (int k = 0; k < 4; k++) {
                i16 ax = static_cast<i16>(m_rand.randr_w(bx));
                if (plus2l) ax = add16(ax, static_cast<i16>(2 * 60));
                ax = add16(ax, W(si));
                SW(di, ax);
                si += 2;
                di += 2;
            }
            bx = static_cast<u16>(bx + step);
            si += 2;
            di += 2;
        }
    };
    jitter(M3(sdsph1), M3(sdsph), 16, 60 * 5, (2 * 240) / 16, false);
    jitter(M3(smsph1), M3(smsph), 16, 60, (2 * 60) / 16, true);
    {   // flsmph: 3 records, range 500, a 1000 offset
        u32 si = M3(flsmph1) + 2, di = M3(flsmph) + 2;
        u16 bx = 500;
        for (int rec = 0; rec < 3; rec++) {
            for (int k = 0; k < 4; k++) {
                i16 ax = static_cast<i16>(m_rand.randr_w(bx));
                ax = add16(ax, 2 * 500);
                ax = add16(ax, W(si));
                SW(di, ax);
                si += 2;
                di += 2;
            }
            bx = static_cast<u16>(bx + (2 * 500) / 16);
            si += 2;
            di += 2;
        }
    }
    m_eng.calcsin(static_cast<u16>(M3(rvec)));
    m_eng.calcmat(static_cast<u16>(M3(rmat)));
}

// ==========================================================================================
// 9536-9779: matrices for the new position, effective coordinates
// ==========================================================================================
void Main3::matricesAfterMove() {
    if (B(M3(ejectflag)) != 0) {   // hardbargain1: the man on foot, and the plane
        m_eng.calcsin(static_cast<u16>(M3(obj5d) + 12));
        m_eng.calcmat(static_cast<u16>(M3(nzmat)));
        m_eng.ncalcmat(static_cast<u16>(M3(nnzmat)));
        m_eng.calcsin(static_cast<u16>(M3(orvec)));
        m_eng.calcmat(static_cast<u16>(lbl::orot));
        m_eng.ncalcmat(static_cast<u16>(lbl::norot));
        for (int k = 0; k < 6; k++) SW(M3(oxeff) + 2 * k, W(M3(obj5d) + 2 * k));
        for (int k = 0; k < 3; k++) SW(M3(orveceff) + 2 * k, W(M3(obj5d) + 12 + 2 * k));
        return;
    }
    m_eng.calcsin(static_cast<u16>(M3(orvec)));
    m_eng.calcmat(static_cast<u16>(lbl::orot));
    m_eng.calcmat(static_cast<u16>(M3(nzmat)));
    m_eng.ncalcmat(static_cast<u16>(lbl::norot));
    for (int k = 0; k < 9; k++) SW(M3(nnzmat) + 2 * k, W(lbl::norot + 2 * k));
    for (int k = 0; k < 6; k++) SW(M3(oxeff) + 2 * k, W(M3(ox) + 2 * k));
    for (int k = 0; k < 3; k++) SW(M3(orveceff) + 2 * k, W(M3(orvec) + 2 * k));
}

// 10008-10043: the lights for 10000 and 20000 feet: on when the altitude word is above 4 / 9
// (the ASM compares the quotient of the low word too, but it can never reach its limit)
void Main3::altitudeLights() {
    drawbutton(static_cast<u16>(static_cast<u16>(W(M3(ozeff) + 2)) > 4 ? 0x0507 : 0x0705), 10);
    drawbutton(static_cast<u16>(static_cast<u16>(W(M3(ozeff) + 2)) > 9 ? 0x0507 : 0x0705), 11);
}

// 10044-10082: too high: lost in space (the SRATS stars program is Phase 9)
bool Main3::highAltitude() {
    if (!(UW(M3(obj5dbits)) & 0x2000)) return false;   // only the bomber goes that high
    const i32 oz = D(M3(ozeff));
    const i16 high = static_cast<i16>((static_cast<u32>(oz) >> 8) & 0xFFFF);   // cmp oxeff+9,ax: altitude / 256
    if (high < W(M3(starheight))) return false;   // below the stars
    if (B(M3(aceflag)) == 0) return false;             // don't do it if not ace
    SB(M3(bhighflag), 1);
    m_world.flushtiles();
    m_eng.drawnear();
    SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) | 0x400));   // lost in space
    SB(M3(exitflag), 0xFF);
    return true;
}

// 10096-10139: stepping back into the plane after getting out and walking away and back
void Main3::getBackIntoPlane() {
    if (B(M3(ejectflag)) == 0) return;
    if (B(M3(exitflag)) != 0) return;
    const u16 si = static_cast<u16>(M3(obj5d));
    if (W(si + 24) == 0) return;   // is the plane drawn?
    u16 dist;
    if (!evaldist(si, dist)) return;
    if (dist > 4000) {
        SB(M3(outfromflag), 0xFF);
        return;
    }
    if (B(M3(outfromflag)) == 0) return;   // have we gotten out of the plane yet?
    if (dist > 2000) return;
    interiorview();
    SB(M3(redamageflag), 0xFF);
    if (UW(M3(totaldamage)) == 0) SB(M3(redamageflag), 0);   // no damage: start fresh
    if (m_snd) m_snd->snareinit();   // turn the missile sound back on
    SB(M3(windflag), 0);              // the wind off
    damageinit();
    redrawbuts();
    for (int k = 0; k < 9; k++) SW(M3(ox) + 2 * k, W(si + 2 * k));   // move us to the plane's orientation
    SW(si + 24, 0);   // remove it from the table
    SB(M3(ejectflag), 0);
    SB(M3(stepoutflag), 0);
}

// ==========================================================================================
// 10143-10232: looking around: the control key (or numlock) turns the cursor keys into views
// ==========================================================================================
void Main3::viewKeys() {
    if (scnIs(0x2F)) {   // 'v': call the rescue vehicle
        clearScn();
        if (B(M3(ejectflag)) != 0 && B(M3(standingflag)) != 0 && W(M3(maxnrvs)) == 0 && B(M3(homeflag)) == 0) {
            rescuestart();
        }
    }
    if (scnIs(0x45)) {   // numlock
        clearScn();
        SB(M3(numlock), B(M3(numlock)) ^ 0xFF);
    }
    for (int k = 0; k < 3; k++) SW(M3(desiredyaw) + 2 * k, 0);
    SB(M3(desireflag), 0);
    if (key(0x1D) || B(M3(numlock)) != 0) {
        SB(M3(desireflag), 0xFF);
        SW(M3(desiredyaw), 0);
        if (key(0x4B)) SW(M3(desiredyaw), 16380);
        if (key(0x4D)) SW(M3(desiredyaw), -16380);
        if (key(0x48)) SW(M3(desiredyaw) + 2, -8192);
        const bool down = key(0x50);
        if (B(M3(ejectflag)) == 0) {
            if (down) SW(M3(desiredyaw) + 2, 8192);
            if (key(0x4C)) SW(M3(desiredyaw) + 2, 16383);
        } else if (down) {
            SW(M3(desiredyaw), 32767);
        }
    } else {
        for (int k = 0; k < 3; k++) SW(M3(curryaw) + 2 * k, 0);   // no views: the view follows the plane
    }
}

} // namespace corncob::game
