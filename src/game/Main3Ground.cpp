// Main3Ground.cpp - Port of the 3.ASM ground contact, landing gear, crash checks and movement
//
// 3.ASM lines 9143-9560, plus the procs they call (findheight, dolandinggear - the mdolandinggear
// macro of 3MACS.INC - brake, qcrashland, qhomebase, moveme, step, dampchute ...).

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
inline i32 neg32(i32 v) { return static_cast<i32>(0u - static_cast<u32>(v)); }
inline i32 add32(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b)); }
inline i32 sub32(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) - static_cast<u32>(b)); }
inline i32 sal32(i32 v, int n) { return static_cast<i32>(static_cast<u32>(v) << n); }
inline i32 mk32(u16 lo, u16 hi) { return static_cast<i32>(static_cast<u32>(lo) | (static_cast<u32>(hi) << 16)); }
inline i16 lo16(i32 v) { return static_cast<i16>(static_cast<u32>(v) & 0xFFFF); }
inline i16 hi16(i32 v) { return static_cast<i16>(static_cast<u32>(v) >> 16); }
inline u32 mul16(u16 a, u16 b) { return static_cast<u32>(a) * b; }
inline i32 cwd(i16 a) { return static_cast<i32>(a); }

constexpr int EHMIN = 720;       // 4*3*l
constexpr int MAXZSPR = 540;
constexpr int ZSPR4 = 0;         // set below from the generated constant

} // namespace

// ==========================================================================================
// 3.ASM findheight: adjusts the height of the wheels; carry (true) when the attitude is a crash
// ==========================================================================================
bool Main3::findheight() {
    SB(M3(crshflg), 5);
    if (static_cast<u16>(abs16(W(M3(orveceff) + 4))) > static_cast<u16>(W(M3(maxroll)))) return true;   // inverted
    SB(M3(crshflg), 7);
    if (static_cast<u16>(abs16(W(M3(orveceff) + 2))) > static_cast<u16>(W(M3(maxpitch)))) return true;  // insane pitch
    SB(M3(crshflg), 0);
    SB(M3(nwheels), 0);
    SW(M3(wingvec), 0);
    SW(M3(tmpsum), 0);
    SW(M3(tmpsum) + 2, 0);
    SW(M3(wingvec) + 2, W(M3(hlow)));
    SW(M3(wingvec) + 4, neg16(W(M3(hmin))));   // how far the wheels extend beneath the plane
    m_eng.matvmul(static_cast<u16>(M3(wingvec)), static_cast<u16>(M3(nnzmat)), static_cast<u16>(M3(xvec)));
    SW(M3(tmpsum), neg16(W(M3(xvec) + 4)));      // depth of this wheel
    SW(M3(wingvec) + 2, neg16(W(M3(hlow))));
    m_eng.matvmul(static_cast<u16>(M3(wingvec)), static_cast<u16>(M3(nnzmat)), static_cast<u16>(M3(xvec)));
    SW(M3(tmpsum) + 2, neg16(W(M3(xvec) + 4)));  // and the other wheel
    SW(M3(bounceheight), std::max(W(M3(tmpsum)), W(M3(tmpsum) + 2)));
    return false;
}

// 3.ASM brake: slows rtopv by force (0-15, per frame time); returns |v| before braking in `mag`,
// and leaves the y delta where the ASM leaves it in ax
void Main3::brake(i16 force, u16& mag, i16& axOut) {
    mag = vmag(static_cast<u16>(M3(rtopv)));
    const i16 f = wtadj(force);
    i16 dx = idiv32(imul16(f, neg16(W(M3(rtopv)))), static_cast<i16>(mag)).q;
    SW(M3(rtopv), add16(W(M3(rtopv)), dx));
    dx = idiv32(imul16(f, neg16(W(M3(rtopv) + 2))), static_cast<i16>(mag)).q;
    SW(M3(rtopv) + 2, add16(W(M3(rtopv) + 2), dx));
    axOut = dx;
}

// 3.ASM killss: kill the side slip
void Main3::killss() {
    SW(M3(topv) + 2, 0);
    m_eng.matvmul(static_cast<u16>(M3(topv)), static_cast<u16>(M3(nnzmat)), static_cast<u16>(M3(rtopv)));
}

// 3.ASM bouncevert: bounces the plane up when it hits the ground
void Main3::bouncevert() {
    i16 ax = W(M3(rtopv) + 4);
    if (ax >= 0) return;   // make sure the velocity is down
    ax = neg16(ax);        // reverse direction: bounce
    i16 bx = ax;
    if (!(UW(M3(topv)) < 3000)) bx = sar16(sar16(bx, 1), 1);   // slow: let the shocks work better
    bx = sar16(bx, 1);
    SW(M3(rtopv) + 4, sub16(ax, bx));
}

// 3.ASM dolandinggear (mdolandinggear, 3MACS.INC): the plane near the ground. Carry (true) means
// the plane crashed; a hard but survivable landing sets crshlndflag instead.
bool Main3::dolandinggear() {
    if (B(M3(crshlndflag)) != 0) return false;
    SW(M3(springforce), 0);
    SB(M3(oldgflag), B(M3(groundflag)));   // assume not on the ground
    SB(M3(groundflag), 0);
    {
        const i16 ax = uwtadj(W(M3(rtopv) + 4));
        SW(M3(oldrtopv4), ax);
    }
    // height above the ground: only the cases where it is zero in the high word are close
    i32 h = sub32(D(M3(ozeff)), D(M3(zground)));
    if (hi16(h) < 0 || hi16(h) != 0) return false;   // in the air
    // the spring: how far into the wheels' travel the plane is
    i32 s = add32(D(M3(zground)), static_cast<i32>(static_cast<u16>(W(M3(bounceheight))) + static_cast<u16>(ZSPR4)));
    s = sub32(s, D(M3(ozeff)));
    if (hi16(s) < 0) return false;                      // the spring is not sprung
    i16 ax = lo16(s);
    bool guyDies = false;
    if (B(M3(onrunwayflag)) == 0) {                     // on the grass: twice as tough
        if (!sf16(ax, MAXZSPR / 2)) {
            ax = static_cast<i16>(static_cast<u16>(ax) << 1);
            guyDies = true;
        }
    } else if (!sf16(ax, MAXZSPR)) {
        guyDies = true;                                  // the spring is sprung
    }
    if (guyDies) {
        if (!sf16(ax, MAXZSPR * 2)) return true;         // guyreallydies
        SB(M3(crshlndflag), 0xFF);
        return false;
    }
    // dolndlb2: touching the ground
    SB(M3(groundflag), 0xFF);
    killss();
    SW(M3(temp4), ax);
    SW(M3(springforce), ax);
    const u16 kick = static_cast<u16>(mul16(static_cast<u16>(ax), static_cast<u16>(d::m3_equ::kspr)) / 1000);
    i16 v = uwtadj(W(M3(rtopv) + 4));
    v = add16(v, static_cast<i16>(kick));
    SW(M3(rtopv) + 4, wtadj(v));
    {   // landing hard rolls the plane: (tmpsum - tmpsum+2) * springforce << 5 into thrdot
        const i16 diff = sub16(W(M3(tmpsum)), W(M3(tmpsum) + 2));
        i32 roll = imul16(diff, W(M3(springforce)));
        dwAdd(M3(thrdotl), M3(thrdoth), sal32(roll, 5));
    }
    // brakes, only with the plane on the ground
    u8 pressed = static_cast<u8>(key(0x34));   // '.'
    if (B(M3(joyflag)) != 0) pressed |= static_cast<u8>(key(0x2A));
    pressed |= static_cast<u8>(key(0x52));      // 0 on the keypad
    i16 force = 15;                              // normal braking force
    if (!pressed) {
        if (UW(M3(rpm)) > 40) return false;      // throttle is on: no braking at all
        force = 6;
    }
    u16 mag;
    i16 ax2;
    brake(force, mag, ax2);
    SW(M3(rtopv) + 2, add16(W(M3(rtopv) + 2), ax2));   // the ASM adds the y delta a second time
    if (mag > static_cast<u16>(wtadj(100))) return false;
    if (UW(M3(rpm)) > 40) return false;           // throttle must be off
    SW(M3(rtopv), 0);
    SW(M3(rtopv) + 2, 0);
    SB(M3(freezeflag), 0xFF);                      // freeze action
    qhomebase();
    const u8 home = B(M3(homeflag));
    if (home) {
        SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) & 0xFFFE));
        SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) | 4));   // landed at home
    }
    SB(M3(homelandflag), home);   // see if we landed on the runway
    return false;
}

// 3.ASM set5dbits: crash land bits in obj5dbits
void Main3::set5dbits() {
    if (UW(M3(obj5dbits)) & 4) return;
    u8 al = static_cast<u8>((B(M3(homeflag)) << 1) & 2);
    if (al == 0) SW(M3(nplaneslost), add16(W(M3(nplaneslost)), 1));   // not home: the plane is a lost cause
    al |= 4;
    SW(M3(obj5dbits), static_cast<i16>(UW(M3(obj5dbits)) | al));
}

// 3.ASM qcrashland: checks for crash landing and takes action
void Main3::qcrashland() {
    if (B(M3(crshlndflag)) == 0) return;
    if (B(M3(ejectflag)) != 0 && W(M3(obj5d) + 24) != 0) {
        set5dbits();
        odest(static_cast<u16>(M3(obj5d)), 0, true);   // destroy the plane
    }
    if (B(M3(invincible)) != 0) {
        resetplane();
        SB(M3(crshlndflag), 0);
        return;
    }
    SW(M3(rpm), 20);
    if (W(M3(currpmx)) == 0) return;   // don't bother if already crashed
    if (m_snd) {                       // the scraping of the belly, a screech and a blam
        m_snd->scrapeinit();
        SB(M3(windflag), 0);
        m_snd->snaresnd();
        voiceon(3, 0x300);
        m_snd->b3laminit();
        voiceoff(2, 0x10);
        voiceon(2, 0x10);
    }
    SW(M3(currpmx), 0);                // kill the throttle during the crash landing
}

// 3.ASM qhomebase: checks for being over home base
void Main3::qhomebase() {
    u8 ah = B(M3(homenowflag));
    u8 al = B(M3(closetowerflag));
    const u8 bl = static_cast<u8>(al >> 1);
    ah |= bl;
    al &= 1;
    ah &= al;
    SB(M3(homenowflag), ah);
    SB(M3(homeflag), ah);
    if (ah != 0) return;
    if (UW(M3(frmnum)) < 20) return;   // give it a couple of seconds
    SB(M3(neverleftflag), 0);          // now show that we have left home
}

// 3.ASM resetplane: sets the plane back up after a fall (the invincible plane)
void Main3::resetplane() {
    SB(M3(crshflg), 0);
    bouncevert();
    const u32 di = B(M3(ejectflag)) ? static_cast<u32>(M3(obj5d)) : static_cast<u32>(M3(ox));
    SW(di + 8, W(M3(hmin)));
    SW(di + 10, 0);   // clear z, pitch and roll
    SW(di + 14, 0);
    SW(di + 16, 0);
}

// 3.ASM pitchlock: verifies that the pitch angle is locked (the autopilot)
bool Main3::pitchlock() {
    if (static_cast<u16>(abs16(W(M3(rtopv) + 4))) < 150) return false;   // looks level: tell them we didn't do anything
    const u16 mag = vmag(static_cast<u16>(M3(rtopv)));
    if (mag < 3900) {   // speed too low: turn off the autopilot, plane unflyable
        SB(M3(autoflag), 0);
        return true;
    }
    i16 v = bndck(static_cast<i16>(mag), 15500, 3901);   // force v into the autopilot's range
    const u32 num = mul16(static_cast<u16>(v), 8);
    const u16 q = static_cast<u16>(num / 15591);         // 0-7
    const u16 rem = static_cast<u16>(num % 15591);        // modulo for interpolation
    const u16 idx = static_cast<u16>((q - 2) * 2);        // 0-10 in steps of 2
    const u32 table = M3(locktable) + idx;
    i16 ax = W(table + 2);
    i16 bx = W(table);
    ax = sub16(ax, bx);
    i16 err = idiv32(imul16(ax, static_cast<i16>(rem)), 15591).q;
    bx = add16(bx, err);                                   // the target orveceff+2
    i16 diff = sub16(W(M3(orveceff) + 2), bx);
    diff = bndck(diff, 3200, -2000);                       // don't wang the plane around
    const i32 num2 = mk32(0, static_cast<u16>(diff));      // the number on top: diff << 16
    const i16 den = 70;                                    // (simpflag: 280 for a stalled/aero plane, off by default)
    dwAdd(M3(pitdotl), M3(pitdoth), sas::divrs_dw(num2, den));
    return true;   // leave flag that pitch is not locked
}

// 3.ASM dohlock: locks the autopilot to an altitude
void Main3::dohlock() {
    // (cmp rtopv+4,15 / ja: signed word compare of the signed velocity, as written)
    if (static_cast<u16>(W(M3(rtopv) + 4)) > 15) {   // velocity too big: unlock
        SB(M3(lockflag), 0);
        return;
    }
    if (B(M3(lockflag)) == 0) {
        SB(M3(lockflag), 0xFF);
        SW(M3(lockheightl), W(M3(ozeff)));
        SW(M3(lockheighth), W(M3(ozeff) + 2));   // save the present height
        return;
    }
    i32 err = sub32(D(M3(ozeff)), mk32(UW(M3(lockheightl)), UW(M3(lockheighth))));   // error in height
    err = neg32(err);
    err = sas::muls_dw(err, 655);                     // now mult the error in distance
    dwAdd(M3(pitdotl), M3(pitdoth), err);             // apply to pitch
}

// 3.ASM dodvelavg: averages dvel to get the g factor (brown out and red out)
void Main3::dodvelavg() {
    if (!(UW(M3(frmnum)) > 3)) return;   // don't do until the 3rd frame
    const i16 bx = uwtadj(W(M3(navgdvel)));
    i32 v = D(M3(dvel) + 8);              // z acceleration velocity
    if (B(M3(ejectflag)) != 0) v = 0;     // if we have ejected we can't black out
    v = sub32(v, dwGet(M3(avgdvell), M3(avgdvelh)));
    dwAdd(M3(avgdvell), M3(avgdvelh), sas::divrs_dw(v, bx));
    i16 ax = bndck(W(M3(avgdvelh)), 100, -100);   // rail it if needed
    SW(M3(avgdvelh), ax);
    ax = sub16(abs16(ax), 60);                    // we can take 30 g's
    if (ax < 0) {
        if (B(M3(fadecolor)) == 255) return;      // are we faded back?
        SB(M3(fadecolor), 255);
        if (!(B(M3(tripflag1)) & 1)) m_eng.setcolors(255);
        return;
    }
    // ax is 0-70, scaled into 255-0
    const u16 f = static_cast<u16>(mul16(static_cast<u16>(ax), 255) / 41);
    SB(M3(fadecolor), static_cast<u8>(static_cast<u16>(0u - f) + 255));
    if (!(B(M3(tripflag1)) & 1)) m_eng.setcolors(B(M3(fadecolor)));
}

// 3.ASM moveme / divbycl: apply the velocity (and half the acceleration) to the position
void Main3::moveme() {
    int cl = 4;   // divide all velocities by 16 before applying to the coordinates
    if (UW(M3(obj5dbits)) & 0x2000) cl = 2;   // put on the gas (the bomber)
    for (int k = 0; k < 3; k++) {
        i16 ax = W(M3(racctop) + 2 * k);   // acceleration
        ax = sar16(ax, 1);                  // /2 for 1/2 a t^2
        ax = add16(ax, W(M3(rtopv) + 2 * k));
        i32 v = dwtadj(cwd(ax));
        v = v >> cl;                        // divbycl: arithmetic shifts of dx:ax
        addToDword(M3(ox) + 4 * k, v);
    }
}

// 3.ASM moveobs: moves the observer by surfacev
void Main3::moveobs() {
    for (int k = 0; k < 3; k++) {
        const i16 ax = sar16(W(M3(surfacev) + 2 * k), 4);
        addToDword(M3(ox) + 4 * k, dwtadj(cwd(ax)));
    }
}

// 3.ASM step / goodstep: take a step forward
void Main3::step(i16 stepsz) {
    const u16 now = m_clock.oticks;
    if (static_cast<i16>(abs16(sub16(static_cast<i16>(now), W(M3(steptick))))) <= 40) return;   // jle nostepnow
    SW(M3(steptick), static_cast<i16>(now));
    SW(M3(orvec), add16(W(M3(orvec)), W(M3(steporvec))));   // a little sway
    SW(M3(steporvec), neg16(W(M3(steporvec))));
    goodstep(stepsz);
}

void Main3::goodstep(i16 stepsz) {
    const i16 pitch = W(M3(orvec) + 2);
    SW(M3(orvec) + 2, 0);
    m_eng.calcsin(static_cast<u16>(M3(orvec)));
    m_eng.ncalcmat(static_cast<u16>(M3(dnrot)));
    SW(M3(orvec) + 2, pitch);
    SW(M3(xvec), stepsz);   // move forward
    SW(M3(xvec) + 2, 0);
    SW(M3(xvec) + 4, 0);
    m_eng.matvmul(static_cast<u16>(M3(xvec)), static_cast<u16>(M3(dnrot)), static_cast<u16>(M3(surfacev)));
    SW(M3(surfacev) + 4, 0);   // no z motion
    moveobs();
    SW(M3(surfacev), 0);
    SW(M3(surfacev) + 2, 0);
    SW(M3(surfacev) + 4, 0);
}

// 3.ASM dampchute: slow the falling man down: air resistance, terminal velocity
void Main3::dampchute() {
    const i16 cd = W(M3(chutedamp));
    for (int k = 0; k < 2; k++) {
        const i32 p = imul16(W(M3(otopv) + 2 * k), cd);
        SW(M3(otopv) + 2 * k, static_cast<i16>(hi16(p) << 1));
    }
    const i16 gravity = static_cast<i16>(d::m3_equ::freefallgrav);
    i32 dxax = dwtadj(imul16(W(M3(otopv) + 4), gravity));   // the lower the frame rate, the higher the drag
    const i16 term = W(M3(termvel));
    const i16 dx = hi16(dxax);
    if (!(static_cast<u16>(abs16(term)) > static_cast<u16>(abs16(dx)))) {   // ja thisdivok
        SW(M3(otopv) + 4, 0);   // just stop us (what else could we do?)
        m_eng.cecode = 99;
        return;
    }
    const i16 q = idiv32(dxax, term).q;   // v = v - kv braking effect
    SW(M3(otopv) + 4, sub16(W(M3(otopv) + 4), q));
}

// 3.ASM jumpadd: adds how long j has been held down to jumpvalue
void Main3::jumpadd() {
    const DosKeyboard::Read r = m_kbd.readkey(0x24);
    i16 ax = static_cast<i16>(r.down);
    ax = add16(ax, W(M3(jumpvalue)));
    ax = bndck(ax, static_cast<i16>(d::m3_equ::jumpmax), 0);
    SW(M3(jumpvalue), ax);
}

// 3.ASM recalcmats
void Main3::recalcmats() {
    m_eng.calcsin(static_cast<u16>(M3(orvec)));
    m_eng.calcmat(static_cast<u16>(M3(orot)));
    m_eng.ncalcmat(static_cast<u16>(M3(norot)));
}

} // namespace corncob::game
