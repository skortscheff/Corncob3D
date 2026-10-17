// Main3Misc.cpp - Port of the rest of the 3.ASM main loop: prop, weapons, throttle, flaps,
// autopilot, ejecting, the parachute, the escape key and the end of the mission
//
// 3.ASM lines 10234-11300. Weapons (bulstrt, strtmis, strtbomb) are Phase 5; the requests are
// made here exactly as the ASM does, with the timers and counters that limit them.

#include "Main3.hpp"
#include "DGroup.hpp"
#include "core/AsmMath.hpp"
#include "engine/AsmTables.hpp"
#include <algorithm>
#include <cstdlib>

namespace corncob::game {

namespace d = corncob::dgroup;
using asmx::bndck;
using asmx::imul16;
using asmx::sf16;
namespace lbl = corncob::lbl;

#define M3(name) lbl::name

namespace {

inline i16 add16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) + static_cast<u16>(b))); }
inline i16 sub16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) - static_cast<u16>(b))); }
inline i16 neg16(i16 a) { return static_cast<i16>(static_cast<u16>(0u - static_cast<u16>(a))); }
inline i32 add32(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b)); }
inline u32 mul16(u16 a, u16 b) { return static_cast<u32>(a) * b; }

constexpr int EHMIN = 720;

} // namespace

// ==========================================================================================
// 10234-10283: the prop, drawn behind the plane, and the gun mode
// ==========================================================================================
void Main3::propAndWeapons() {
    // move the prop to the right place behind the plane
    for (int k = 0; k < 9; k++) SW(M3(objprop) + 2 * k, W(M3(ox) + 2 * k));
    SW(M3(propvec) + 4, -900);
    m_eng.matvmul(static_cast<u16>(M3(propvec)), static_cast<u16>(lbl::norot), static_cast<u16>(M3(xvec)));
    for (int k = 0; k < 3; k++) addToDword(M3(objprop) + 4 * k, static_cast<i32>(W(M3(xvec) + 2 * k)));
    {
        const u16 bx = static_cast<u16>(static_cast<u16>(sub16(W(M3(rpm)), 10)) << 1);
        SW(M3(objprop) + 16, static_cast<i16>(0u - mul16(m_clock.oticks, bx)));   // the prop speed as an angle
    }

    // cannons if alt (shift) is down, or when standing: short, slow bullets
    SW(M3(bulvel), 4400);
    SW(M3(bulrate), 2);
    SW(M3(bullife), 96);
    if (B(M3(ejectflag)) != 0 || key(0x2A)) {
        SW(M3(bulrate), 1);
        SW(M3(bullife), 48);   // only half a life
        SW(M3(bulvel), 700);
    }

    // the low altitude warning, with some hysteresis
    bool altfine = true;
    if (W(M3(oz) + 2) == 0) {   // oz+2 == 0: not way high
        u16 limit = 4500;
        if (B(M3(altlighton)) != 0) limit = 5000;   // if the light is on, don't turn off until higher
        if (UW(M3(oz)) < limit && B(M3(groundflag)) == 0 && B(M3(ejectflag)) == 0) {
            altfine = false;
            flashalt();
            if (B(M3(altmsgflag)) != 0 && B(M3(closetowerflag)) == 0 && B(M3(altlighton)) == 0 &&
                !(UW(M3(oz)) < 2000))
                SW(M3(rmsgqueue), static_cast<i16>(M3(hitrmsg)));   // this avoids the message at takeoff
            SB(M3(altlighton), 0xFF);
        }
    }
    if (altfine) SB(M3(altlighton), 0);

    // ---- bullets ----
    bool wantBullet = false;
    if (UW(M3(bultick)) == 0) {   // the bullet timer has run down
        SW(M3(bultick), W(M3(bulrate)));
        if (W(M3(bulavg)) != 0) SW(M3(bulavg), static_cast<i16>(W(M3(bulavg)) - 1));
        if (W(M3(misavg)) != 0) SW(M3(misavg), static_cast<i16>(W(M3(misavg)) - 1));
        // keyboard (the joystick buttons come with Phase 6)
        bool skipcann = false;
        if (W(M3(ozeff) + 2) == 0) {   // are we low?
            if (UW(M3(ozeff)) < 2 * EHMIN) skipcann = true;                       // really low
            else if (B(M3(homeflag)) != 0 && !(UW(M3(ozeff)) > 15 * EHMIN)) skipcann = true;   // low and home: no cannons
        }
        if (!skipcann && key(0x2A)) wantBullet = true;   // alt shoots cannons
        else if (key(0x39)) wantBullet = true;           // the space bar
    }
    if (wantBullet && !(UW(M3(bulavg)) > 32)) {   // 32 bullets out?
        SB(M3(homelandflag), 0);
        bulstrt();
    }

    // ---- missiles ----
    if (UW(M3(mistick)) == 0) {
        SW(M3(mistick), W(M3(misrate)));
        if (key(0x2E)) {   // 'c'
            if (!(UW(M3(misavg)) > 16) && B(M3(crshlndflag)) == 0) {
                SB(M3(homelandflag), 0);
                bool ok = B(M3(ejectflag)) == 0 || B(M3(remoteflag)) != 0;   // out of the plane needs the caps lock
                if (ok && !strtmis()) SW(M3(misavg), add16(W(M3(misavg)), 5));
            }
        }
    }

    // ---- sound toggle ----
    if (scnIs(0x1F)) {   // 's'
        clearScn();
        SB(M3(sndflg), static_cast<u8>(B(M3(sndflg)) ^ 0xFF));
    }
    if (scnIs(0x0A)) SB(M3(cheatflag), 0xFF);   // '9' (testerflag)

    // ---- the rear rocket: charges up with time, 'i' ignites it ----
    {
        SB(M3(oprclr1), static_cast<u8>(1 + B(M3(rocketflag))));
        const u16 ticks = m_clock.ticks;
        const u32 elapsed = static_cast<u16>(ticks - UW(M3(lrockettick)));
        const u32 prod = elapsed * static_cast<u16>(W(M3(ofrmticks)));   // correct for advanced time
        const u16 real = static_cast<u16>(W(M3(realofrmticks)));
        bool charged = false;
        if ((prod >> 16) >= real) {   // time went wrong
            SW(M3(lrockettick), static_cast<i16>(ticks));
        } else {
            const u16 q = static_cast<u16>(prod / real);
            SW(M3(lrockettick), static_cast<i16>(ticks));
            const u16 sum = static_cast<u16>(q + UW(M3(rockettick)));
            if (sum > 2184) charged = true;
            else SW(M3(rockettick), static_cast<i16>(sum));
        }
        if (charged) {
            SB(M3(oprclr1), 0x0A);   // make the needle green
            if (!key(0x38) && (B(M3(ejectflag)) == 0 || B(M3(remoteflag)) != 0) && scnIs(0x17))
                SB(M3(rocketflag), 0x0B);   // fire the rocket: the display goes red
        }
    }

    // ---- bombs ----
    if (B(M3(nbombs)) < 3) {   // make a bomb if it is time
        const u16 ticks = m_clock.ticks;
        const u16 since = static_cast<u16>(ticks - UW(M3(lastbombtick)));
        const u32 prod = mul16(since, static_cast<u16>(W(M3(ofrmticks))));
        u16 ax;
        if ((prod >> 16) < static_cast<u16>(W(M3(ofrmticks)))) ax = static_cast<u16>(prod / static_cast<u16>(W(M3(realofrmticks))));
        else ax = since;
        if (!(ax < UW(M3(bombwait)))) {
            SB(M3(nbombs), static_cast<u8>(B(M3(nbombs)) + 1));
            drawbutton(0, 6 + B(M3(nbombs)) - 1);
            SW(M3(lastbombtick), static_cast<i16>(ticks));
        }
    }
    bool plant = scnIs(0x27);   // ';' plants a bomb
    if (scnIs(0x30)) {          // 'b': drop a bomb
        clearScn();
        if (B(M3(nbombs)) != 0 && (B(M3(ejectflag)) == 0 || B(M3(remoteflag)) != 0)) {
            strtbomb();
            SB(M3(nbombs), static_cast<u8>(B(M3(nbombs)) - 1));
            drawbutton(0, 6 + B(M3(nbombs)));
            SW(M3(lastbombtick), static_cast<i16>(m_clock.ticks));
        }
    } else if (!plant && key(0x38)) {   // alt
        if (scnIs(0x17)) {   // alt-i: invincible (shareware: one way)
            clearScn();
            SB(M3(invincible), 0xFF);
            cleardamage();
            restoreview();
        }
        if (scnIs(0x19)) plant = true;   // alt-p
    }
    if (plant) {
        clearScn();
        if (B(M3(ejectflag)) != 0 && B(M3(abombsleft)) != 0) {
            // strtbomb1 and the assassin bomb setup: Phase 5
            SB(M3(abombsleft), static_cast<u8>(B(M3(abombsleft)) - 1));
        }
    }

    if (scnIs(0x3A)) {   // caps lock: remote mode
        clearScn();
        SB(M3(remoteflag), B(M3(remoteflag)) ^ 0xFF);
    }
}

// ==========================================================================================
// 10685-10739: the throttle
// ==========================================================================================
void Main3::throttleKeys() {
    if (B(M3(ejectflag)) != 0 && B(M3(remoteflag)) == 0) return;   // no throttle on foot
    if (key(0x0D) || key(0x4E)) {   // '+'
        bool noplus = false;
        if ((UW(M3(obj5dbits)) & 0x2000) && B(M3(aceflag)) == 0) noplus = true;   // the superplane needs an ace
        if (!noplus) {
            if (W(M3(currpmx)) != 0) {
                SB(M3(freezeflag), 0);   // a stopped plane gets moving again
                SB(M3(helpflashflag), 0);
                SW(M3(rpm), add16(W(M3(rpm)), 20));
                if (sf16(W(M3(currpmx)), W(M3(rpm)))) SW(M3(rpm), sub16(W(M3(currpmx)), 3));
            } else if ((UW(M3(obj5dbits)) & 0x100) && B(M3(ejectflag)) == 0) {
                SB(M3(crshflg), 8);   // pushing the throttle of a destroyed plane
            }
        }
    }
    if (key(0x0C) || key(0x4A)) {   // '-'
        SW(M3(rpm), sub16(W(M3(rpm)), 20));
        if (sf16(W(M3(rpm)), 20)) SW(M3(rpm), 20);
    }
}

// ==========================================================================================
// 10741-11165: crash, walking, flaps, autopilot, eject, chute, windows and the function keys
// ==========================================================================================
void Main3::miscKeys() {
    if (scncode() == 0xA2) {   // 'g': the grid
        clearScn();
        SB(M3(gdflg), static_cast<u8>(B(M3(gdflg)) ^ 0xFF));
    }
    if (B(M3(ejectflag)) != 0 && key(0x4C)) step(wtadj(800));       // running
    if (B(M3(ejectflag)) != 0 && key(0x52)) step(wtadj(-800));      // a step backward

    // flaps ('f')
    if (scncode() == 0xA1) {
        clearScn();
        i16 ax = 200;
        SB(M3(flapflag), static_cast<u8>(B(M3(flapflag)) ^ 0xFF));
        if (B(M3(flapflag)) != 0) {   // flaps on
            drawflapbut();
            ax = neg16(ax);
        } else {                       // off: the button goes back to brown
            drawbutton(0, 2);
        }
        if (B(M3(groundflag)) == 0) SW(M3(pitdoth), add16(W(M3(pitdoth)), ax));   // a kick in the pants
    }
    if (static_cast<int8_t>(B(M3(cmpltflag))) < 0) {
        drawcmpltbut();
        SB(M3(cmpltflag), 1);
    }
    // the rear window ('r')
    if (scncode() == 0x13 + 0x80 && W(M3(ptrgwinf)) == static_cast<i16>(M3(gwinf))) {
        clearScn();
        if (W(M3(ptrgwint)) != 0) {
            SW(M3(ptrgwint), 0);
            if (m_viewChanged) m_viewChanged(false);
        } else {
            SW(M3(ptrgwint), static_cast<i16>(M3(gwint)));
        }
    }
    // the autopilot ('a')
    if (scnIs(0x1E)) {
        clearScn();
        SB(M3(autoflag), static_cast<u8>(B(M3(autoflag)) ^ 0xFF));
        if (B(M3(autoflag)) != 0) drawautobut();
    }
    // eject ('e')
    if (scnIs(0x12)) {
        clearScn();
        if (B(M3(ejectflag)) == 0) eject();
    }
    // the parachute: the space bar
    if (scncode() == 0xB9) {
        clearScn();
        if (B(M3(ejectflag)) != 0 && B(M3(pullstrflag)) == 0) SB(M3(pullstrflag), 1);   // pull the string
    }
    if (scnIs(0x26)) {   // 'l'
        clearScn();
        fullsize();
    }
    // function keys: the boss key, the tower, the score, the map (front end: Phase 8)
    if (scnIs(0x3B)) { clearScn(); SB(M3(bossflag), 0xFF); }
    if (scnIs(0x40)) { clearScn(); SB(M3(bossflag), 0x7D); }
    if (scnIs(0x3D)) { clearScn(); SB(M3(bossflag), 0x7F); }
    if (scnIs(0x3C)) { clearScn(); SB(M3(intowerflag), 1); }
    if (scnIs(0x3F)) { clearScn(); }
    if (B(M3(bossflag)) != 0 || B(M3(intowerflag)) != 0) {
        // dotower runs TOWER.EXE (briefings, the boss screen): recreated in Phase 8
        SB(M3(bossflag), 0);
        SB(M3(intowerflag), 0);
    }
    if (UW(M3(frmnum)) == 7) {   // the popup that tells how to get a mission
        if (B(M3(readfileflag)) == 0) SW(M3(rmsgqueue), static_cast<i16>(M3(trainrmsg)));
        else if (W(M3(rpm)) == 20) SW(M3(rmsgqueue), static_cast<i16>(M3(hitf2rmsg)));
    }
    if (scnIs(0x3C)) { clearScn(); SB(M3(intowerflag), 1); }
    if (key(0x23) && B(M3(ejectflag)) != 0) SB(M3(haltflag), 0xFF);
}

// ==========================================================================================
// 10905-11050: ejecting (also how the plane is parked when the mission ends)
// ==========================================================================================
void Main3::eject() {
    // (the sounds off, the wind on: Phase 7)
    SW(M3(ammoleft), 20);          // twenty shots of ammo is all we can carry
    SB(M3(abombsleft), 1);         // an assassin bomb
    SB(M3(numlock), 0);            // turn views off
    SB(M3(outfromflag), 0);        // don't let us in until we've gone out
    SB(M3(redamageflag), 0xFF);    // use the old damage
    const u16 bx = static_cast<u16>(M3(obj5d));
    SW(bx + 24, 0x11);             // make the object real
    SW(bx + 24, static_cast<i16>(UW(bx + 24) | UW(M3(obj5dbits))));
    m_eng.tbadd(static_cast<u16>(W(static_cast<u32>(lbl::tblist))), bx);
    SB(M3(ejectflag), static_cast<u8>(B(M3(ejectflag)) + 1));
    SB(M3(fadecolor), 255);
    if (!(B(M3(tripflag1)) & 1)) m_eng.setcolors(255);   // if you were pulling g's, recover instantly
    for (int k = 0; k < 6; k++) SW(bx + 2 * k, W(M3(ox) + 2 * k));
    for (int k = 0; k < 3; k++) SW(bx + 12 + 2 * k, W(M3(orvec) + 2 * k));
    for (int k = 0; k < 3; k++) SW(M3(oorvec) + 2 * k, W(M3(orvec) + 2 * k));
    bool juststand = false;
    if (B(M3(crshlndflag)) != 0) {   // crash landed: the plane stays where it is
        set5dbits();
        SW(bx + 24, 0xE1);
        SW(bx + 24, static_cast<i16>(UW(bx + 24) | UW(M3(obj5dbits))));
        SW(bx + 24, static_cast<i16>(UW(bx + 24) & ~8));
        const i16 sm = static_cast<i16>(M3(plnplyobjsm));
        SW(bx + 40, sm);
        SW(bx + 42, sm);
        SW(bx + 44, sm);
        SW(bx + 8, static_cast<i16>(UW(M3(hmin)) >> 1));   // drop the plane down to the ground
        SW(bx + 10, 0);
        SD(bx + 8, add32(D(bx + 8), D(M3(zground))));
        SB(M3(freezeflag), 0xFF);
        juststand = true;
    } else if (B(M3(groundflag)) != 0) {
        juststand = true;   // on the ground: just get out of the plane
    }
    if (juststand) {
        const i32 z = add32(static_cast<i32>(static_cast<u16>(W(M3(hmin)))), D(M3(zground)));
        SD(M3(myzground), D(M3(zground)));   // if the plane is on a suspended airstrip, so be it
        SD(M3(oz), z);                       // put you on the ground
        SW(bx + 24, static_cast<i16>(UW(bx + 24) & ~8));   // make the plane non-destructable
        SB(M3(stepoutflag), 0xFF);
        SD(M3(oy), add32(D(M3(oy)), 2000));   // get away from the plane
        for (int k = 0; k < 3; k++) SW(M3(otopv) + 2 * k, 0);
        SB(M3(standingflag), 0xFF);
    } else {   // out in the air
        if (B(M3(exitflag)) == 0) SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) | 0x20));   // eject in air
        SB(M3(standingflag), 0);
        for (int k = 0; k < 3; k++) SW(M3(otopv) + 2 * k, W(M3(rtopv) + 2 * k));
        SW(M3(tmbvec), static_cast<i16>((m_rand.randr_w(8192) & 0xEFFF) | 0x0800));   // make sure no magnitude is too small
        SW(M3(tmbvec) + 2, static_cast<i16>((m_rand.randr_w(8192) & 0xEFFF) | 0x0800));
    }
    fullsize();   // standnext
    SW(M3(chutedamp), 32000);
}

// ==========================================================================================
// 11166-11218: the escape key and the other ways out. Returns whether the loop goes on.
// ==========================================================================================
bool Main3::exitCheck() {
    bool leave = false;
    if (scncode() == 0x81) {   // escape released
        // the first escape in the air only warns (unless messages are off or we are at the tower)
        if (B(M3(rmsgflag)) != 0 && B(M3(closetowerflag)) == 0 && B(M3(escwarnflag)) == 0) {
            SB(M3(escwarnflag), 0xFF);
            SW(M3(rmsgqueue), static_cast<i16>(B(M3(ejectflag)) ? M3(escgwarningrmsg) : M3(escwarningrmsg)));
        } else {
            leave = true;
        }
    }
    if (!leave) {
        if (B(M3(exitflag)) != 0) leave = true;
        else {
            SB(M3(abortflag), 0xFF);   // assume abort
            if (key(0x1D) && key(0x2E)) leave = true;   // ctrl-c
            else {
                SB(M3(abortflag), 0);
                if (m_eng.cecode != 0) leave = true;
                else if (B(M3(rescueflag)) != 0) SB(M3(haltflag), 0), leave = true;
                else if (B(M3(capturedflag)) != 0) {
                    SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) | 8));   // set captured bit
                    leave = true;
                }
            }
        }
    }
    if (!leave) return true;   // bigloop
    return !endMission();
}

// ==========================================================================================
// 11219-11290: the mission is over: the status bits, and parking the plane when leaving by escape
// ==========================================================================================
bool Main3::endMission() {
    if (B(M3(abortflag)) != 0) {
        SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) | 0x4000));   // abort bit
        m_done = true;
        return true;
    }
    if (B(M3(cheatflag)) == 0 && B(M3(invincible)) != 0)
        SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) | 0x2000));
    if (B(M3(exitflag)) == 0) {
        u16 ax = 0;
        SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) & ~1));   // clear the abort bit
        if (B(M3(homeflag)) != 0) {
            ax = 1;
            if (B(M3(neverleftflag)) == 0) {
                ax = 0;
                SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) | 0x10));   // made it home
            }
        }
        SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) | ax));
        if (B(M3(rescueflag)) != 0) {
            SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) | 0x100));   // we were rescued
            SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) & ~8));
        }
        if (B(M3(readfileflag)) != 0 && B(M3(bhighflag)) == 0 && B(M3(crshflg)) == 0 && B(M3(ejectflag)) == 0 &&
            B(M3(abortflag)) == 0) {
            // in the plane at the end of a mission: eject first so the plane is parked, then leave
            SB(M3(exitflag), 0xFF);
            clearScn();
            eject();
            m_done = true;
            return true;
        }
    }
    return finishMission() ? true : true;
}

// 11275-11290: parked or not, the mission ends here
bool Main3::finishMission() {
    if (W(M3(obj5d) + 24) != 0) {   // is the plane even on?
        SW(M3(obj5d) + 72, 2);      // turn it into the parked plane
        SW(M3(obj5d) + 24, static_cast<i16>(UW(M3(obj5d) + 24) | UW(M3(obj5dbits)) | 0x10));   // a timed object
        SW(M3(obj5d) + 52, 32000);   // make it last long
        m_eng.drawnear();           // sort and farm once more
    }
    SW(M3(score), static_cast<i16>(m_clock.frmticks));   // the speed info
    m_done = true;
    return false;
}

// 3.ASM crash: the plane crashed (the animation of the original is Phase 5/8)
void Main3::crash() {
    if (B(M3(invincible)) != 0) {
        resetplane();
        return;
    }
    // (sound, the red crash screen)
    SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) & 0xFFFE));   // clear the abort bit
    SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) | 2));        // set the killed bit
    SW(M3(nplaneslost), add16(W(M3(nplaneslost)), 1));
}

} // namespace corncob::game
