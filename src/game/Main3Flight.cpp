// Main3Flight.cpp - Port of the 3.ASM flight model: controls, rotation rates, forces, velocity
//
// 3.ASM lines 8159-9141. Dword pairs are (low word, high word) variables in DGROUP, often not
// next to each other (ctydoth/ctydotl, avgdvelh/avgdvell), so they go through dwGet/dwAdd.
// Where the ASM does something odd (a "cwd / xchg" to widen, dx loaded from ax instead of sign
// extended before a shift, a value overwritten by a later mov), the C++ does the same thing and
// says so.

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
inline i16 sal16(i16 a, int n) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) << n)); }
inline i32 neg32(i32 v) { return static_cast<i32>(0u - static_cast<u32>(v)); }
inline i32 add32(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b)); }
inline i32 sub32(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) - static_cast<u32>(b)); }
inline i32 sal32(i32 v, int n) { return static_cast<i32>(static_cast<u32>(v) << n); }
inline i32 mk32(u16 lo, u16 hi) { return static_cast<i32>(static_cast<u32>(lo) | (static_cast<u32>(hi) << 16)); }
inline i16 lo16(i32 v) { return static_cast<i16>(static_cast<u32>(v) & 0xFFFF); }
inline i16 hi16(i32 v) { return static_cast<i16>(static_cast<u32>(v) >> 16); }
// "cwd": dx:ax = ax sign extended
inline i32 cwd(i16 ax) { return static_cast<i32>(ax); }
// "mov dx,ax / sar dx,1 / rcr ax,1 ..." with dx loaded from ax, not sign extended: the pair
// (dx:ax) = (ax:ax) shifted right arithmetically n times
inline i32 dupShiftRight(i16 ax, int n) { return mk32(static_cast<u16>(ax), static_cast<u16>(ax)) >> n; }
// "cwd / xchg dx,ax / shifts": the damage word as the high word, its sign as the low word
inline i32 damageWide(i16 v, int shift) { return mk32(v < 0 ? 0xFFFF : 0, static_cast<u16>(v)) >> shift; }
// 16-bit unsigned MUL
inline u32 mul16(u16 a, u16 b) { return static_cast<u32>(a) * b; }

} // namespace

i32 Main3::dwGet(u32 lo, u32 hi) const { return mk32(UW(lo), UW(hi)); }
void Main3::dwSet(u32 lo, u32 hi, i32 v) {
    SW(lo, lo16(v));
    SW(hi, hi16(v));
}
void Main3::dwAdd(u32 lo, u32 hi, i32 v) { dwSet(lo, hi, add32(dwGet(lo, hi), v)); }

// ==========================================================================================
// 3.ASM joytd: the stick, 0..400h centered at 200h. Keyboard: the arrow keys scaled by how long
// they were held; reading a joystick comes with Phase 6.
// ==========================================================================================
void Main3::joytd(i16& ax, i16& bx) {
    ax = 0;
    bx = 0;
    if (B(M3(desireflag)) == 0) {   // not looking at views: the arrows are the stick
        const u16 cx = 0x200;
        const i16 left = static_cast<i16>(m_kbd.scalekey(0x4B, cx));
        const i16 right = static_cast<i16>(m_kbd.scalekey(0x4D, cx));
        ax = add16(neg16(left), right);
        const i16 up = static_cast<i16>(m_kbd.scalekey(0x48, cx));
        const i16 down = static_cast<i16>(m_kbd.scalekey(0x50, cx));
        bx = add16(neg16(up), down);
    }
    if (m_padActive && ax == 0 && bx == 0) {   // no arrow keys: the analog stick (the joystick path)
        ax = static_cast<i16>(m_padRoll * 0x200);
        bx = static_cast<i16>(m_padPitch * 0x200);
    }
    ax = add16(ax, 0x200);
    bx = add16(bx, 0x200);
}

// 3.ASM bumpangles: bx has pitch, cx has yaw, made independent of the cpu speed
void Main3::bumpangles(i16 bx, i16 cx) {
    SW(M3(orvec) + 2, add16(W(M3(orvec) + 2), sal16(wtadj(sar16(bx, 1)), 1)));
    SW(M3(orvec), add16(W(M3(orvec)), sal16(wtadj(sar16(cx, 1)), 1)));
}

// 3.ASM swing: swing the chute in the air
void Main3::swing() {
    i16 ax, bx;
    joytd(ax, bx);
    ax = sub16(ax, 0x200);
    bx = sub16(bx, 0x200);
    if (static_cast<u16>(add16(abs16(ax), abs16(bx))) > 0x20) {   // the stick stops the swinging
        SW(M3(swingcount), 0);
        return;
    }
    // (the ASM's "oz - myzground below hmin" test always takes okbaby: dx >= 0 jumps, and
    // dx < 0 is nonzero so the next jnz jumps too)
    const u16 count = UW(M3(swingcount));
    const u16 swingmx = static_cast<u16>(d::m3_equ::swingmx);
    if (count > swingmx) {   // noswingyet
        SW(M3(swingcount), static_cast<i16>(std::max<int>(count - m_refSteps, swingmx)));
        return;
    }
    if (count == swingmx) {  // swingnowrdy: swinging done, back to the angles the pilot had
        SW(M3(tmbvec), 0);
        SW(M3(tmbvec) + 2, 0);
        for (int k = 0; k < 3; k++) SW(M3(orvec) + 2 * k, W(M3(oorvec) + 2 * k));
        SW(M3(swingcount), static_cast<i16>(count - 1));
        return;
    }
    i16 adj = sub16(static_cast<i16>(count), sar16(static_cast<i16>(swingmx), 1));
    if (adj < 0) adj = 0;   // give zero if the argument is negative
    const u32 prod = mul16(static_cast<u16>(adj), 900);
    const i16 c = engine::scos(static_cast<i16>(prod & 0xFFFF));
    const i32 p = imul16(static_cast<i16>(adj), c);   // count * cosine
    const i16 factor = static_cast<i16>(frameScale(sal16(hi16(p), 1)));   // sal dx,1
    SW(M3(orvec) + 2, add16(W(M3(orvec) + 2), factor));
    SW(M3(orvec) + 2, add16(W(M3(orvec) + 2), factor));
    SW(M3(orvec) + 2, static_cast<i16>(decayStep(W(M3(orvec) + 2), 31.0 / 32.0)));
    SW(M3(swingcount), static_cast<i16>(std::max<int>(count - m_refSteps, 0)));
}

// ==========================================================================================
// 8159-8199: what the stick does. Returns the stick position that drives the plane (roll, pitch).
// Ejected and not remote controlling: the stick looks around instead and the plane gets nothing.
// ==========================================================================================
void Main3::sticks(i16& roll, i16& pitch) {
    i16 ax, bx;
    joytd(ax, bx);
    ax = sub16(ax, 0x200);   // center it
    bx = sub16(bx, 0x200);
    if (B(M3(ejectflag)) != 0 && B(M3(remoteflag)) == 0) {   // outahere: look around
        const i16 cx = neg16(ax);
        bx = neg16(bx);
        for (int k = 0; k < 2; k++) {   // let the tumble die away: 9500/10100 per frame
            SW(M3(tmbvec) + 2 * k, static_cast<i16>(decayStep(W(M3(tmbvec) + 2 * k), 9500.0 / 10100.0)));
        }
        bx = add16(bx, W(M3(tmbvec) + 2));
        bumpangles(bx, cx);
        i16 r = neg16(W(M3(orvec) + 4));
        r = wtadj(sar16(r, 6));
        SW(M3(orvec) + 4, add16(W(M3(orvec) + 4), r));
        if (W(M3(swingcount)) != 0) swing();
        ax = 0;   // zeros for control
        bx = 0;
    }
    roll = ax;
    pitch = bx;
}

// ==========================================================================================
// 8204-8776: how the stick, the rudder and the autopilot change the rotation rates, with the
// stall, ground and damage effects. Everything is a rate (thrdot = roll, pitdot, yawdot) kept as a
// dword; the rates are then turned into a rotation of zmat and the angles read back out of it.
// ==========================================================================================
void Main3::flightDynamics(i16 stickRoll, i16 stickPitch) {
    i16 bx = add16(stickPitch, 5);   // give a plus bias on the joystick pitch
    SW(M3(xang), stickRoll);
    SW(M3(oyrail), W(M3(yraild)));
    SW(M3(yang), bx);
    {
        const i16 clamped = bndck(bx, 0x220, -0x220);   // test for railed
        SW(M3(yraild), sub16(clamped, bx));              // non-zero means joystick railed
    }
    {
        const i16 x = W(M3(xang));
        const i16 clamped = bndck(x, 0x110, -0x110);     // look for semi-railed x
        if (sub16(clamped, x) != 0 && B(M3(groundflag)) != 0 && B(M3(ejectflag)) == 0 &&
            B(M3(altmsgflag)) != 0) {
            if (B(M3(oldxrailflag)) == 0) {
                SW(M3(rmsgqueue), static_cast<i16>(M3(rudrmsg)));
                SB(M3(oldxrailflag), 1);
            }
        } else {
            SB(M3(oldxrailflag), 0);
        }
    }

    const i16 vpxp = bndck(W(M3(ctopv)), W(M3(vstal2)), W(M3(mvstal2)));   // forward limited velocity
    SW(M3(vpxp), vpxp);

    // ---- roll rate (thrdot): stick * speed^2, damped, divided by the moment of inertia ----
    {
        // controls get less sensitive at low air pressure: rhotop/rho0
        i16 q = idiv32(imul16(W(M3(rhotop)), W(M3(xang))), W(M3(rho0))).q;
        i32 v = imul16(q, vpxp);                 // speed times stick position
        v = sas::muls_dw(v, vpxp);               // times vpxp squared
        v = sas::divs_dw(v, 300);                // divided by max vpxp allowed
        v = sas::muls_dw(v, W(M3(thrsf)));       // apply the scale factor
        const i32 R = dwGet(M3(thrdotl), M3(thrdoth));
        // R * 3 / 2: "mult by 3, divide by 2" with 32 bit wrap
        const i32 r3 = static_cast<i32>(static_cast<u32>(R) * 3u);
        const i32 half = r3 >> 1;
        const i32 num = sub32(v, half);
        dwAdd(M3(thrdotl), M3(thrdoth), frameScale(sas::divrs_dw(num, W(M3(thrmi)))));
    }
    if (W(M3(thrdam)) != 0) {   // wing damage: a roll that fades out with airspeed
        i32 v = damageWide(W(M3(thrdam)), 3);
        v = sas::muls_dw(v, vpxp);
        v = sas::divs_dw(v, 300);
        dwAdd(M3(thrdotl), M3(thrdoth), frameScale(v));
    }
    if (B(M3(autoflag)) != 0) {   // autopilot levels the wings
        i16 ax = neg16(W(M3(orveceff) + 4));   // look at roll, reverse feedback
        ax = bndck(ax, 25, -25);               // maximum autopilot adjust rate
        dwAdd(M3(thrdotl), M3(thrdoth), frameScale(dupShiftRight(ax, 2)));
    }

    // ---- side wind: the angle of the side velocity limits how far the weathercock effect goes ----
    {
        const i16 ang = atn2(W(M3(topv)), W(M3(topv) + 2));
        const i16 clamped = bndck(ang, 150, -150);
        if (clamped == ang) {   // nice and small (the ASM computes a value here, then zeroes it)
            SW(M3(ctydoth), 0);
            SW(M3(ctydotl), 0);
        } else {
            SW(M3(ctydoth), sar16(clamped, 4));
            SW(M3(ctydotl), clamped < 0 ? -1 : 0);
        }
    }

    // ---- yaw rate (yawdot): rudder keys ----
    i16 rudder;
    {
        const u16 cx = 0x200;
        i16 di = static_cast<i16>(m_kbd.scalekey(0x2C, cx));   // z
        di = add16(di, neg16(static_cast<i16>(m_kbd.scalekey(0x2D, cx))));   // x
        di = add16(di, static_cast<i16>(m_kbd.scalekey(0x47, cx)));          // home / keypad 7
        di = add16(di, neg16(static_cast<i16>(m_kbd.scalekey(0x49, cx))));   // pgup / keypad 9
        if (m_padActive) di = bndck(add16(di, static_cast<i16>(m_padRudder * 0x200)), 0x400, -0x400);
        rudder = di;
    }
    SW(M3(rdrang), rudder);
    {
        i16 bx2 = W(M3(yawsf));   // apply the scale factor
        if (B(M3(groundflag)) != 0 && !(UW(M3(topv)) > 1500)) bx2 = sal16(sal16(bx2, 1), 1);   // rudder does more on the ground at low speed
        i32 v = imul16(vpxp, rudder);   // speed times stick position
        v = sas::muls_dw(v, bx2);
        v = sub32(v, dwGet(M3(yawdotl), M3(yawdoth)));            // exponential fall off
        v = add32(v, dwGet(M3(ctydotl), M3(ctydoth)));            // side wind
        v = add32(v, imul16(neg16(W(M3(sidef))), 1053));          // weathercock effect
        dwAdd(M3(yawdotl), M3(yawdoth), frameScale(sas::divrs_dw(v, W(M3(yawmi)))));
    }
    if (W(M3(yawdam)) != 0) {
        i32 v = damageWide(W(M3(yawdam)), 3);
        v = sas::muls_dw(v, vpxp);
        v = sas::divs_dw(v, 300);
        dwAdd(M3(yawdotl), M3(yawdoth), frameScale(v));
    }
    // turning correction for auto-ruddering turns
    {
        const i16 h1 = hmul(W(M3(sssin)), W(M3(sscos)));   // angular info
        const i16 h2 = hmul(h1, 3859);                       // scale factor involving g, pi, etc
        const i16 ctopv = W(M3(ctopv));
        if (!(ctopv < 30)) {                                 // don't carry to extreme low speed
            const i32 v = static_cast<i32>(static_cast<u32>(static_cast<u16>(h2)) << 16);
            dwAdd(M3(yawdotl), M3(yawdoth), frameScale(sas::divrs_dw(v, neg16(ctopv))));
        }
    }

    // ---- pitch rate (pitdot) ----
    {
        i16 q = idiv32(imul16(W(M3(rhotop)), W(M3(yang))), W(M3(rho0))).q;
        i32 v = imul16(q, vpxp);
        v = sas::muls_dw(v, W(M3(pitsf)));
        v = sub32(v, dwGet(M3(pitdotl), M3(pitdoth)));   // exponential fall off
        dwAdd(M3(pitdotl), M3(pitdoth), frameScale(sas::divrs_dw(v, W(M3(pitmi)))));
    }
    // (simpflag: "if aeroflag and not stalled call pitchlock" - aeroflag is the -y option, off)
    if (B(M3(aeroflag)) != 0 && B(M3(qstald)) == 0) pitchlock();

    if (B(M3(autoflag)) != 0) {
        if (!pitchlock()) {   // pitch is locked: now the vertical velocity and the height
            i16 ax = sub16(W(M3(topv) + 4), W(M3(oldzvel)));
            ax = bndck(ax, 150, -150);
            const i32 A = dupShiftRight(ax, 3);
            i16 r = neg16(W(M3(rtopv) + 4));   // look at roll
            r = bndck(r, 50, -50);
            const i16 absr = abs16(r);
            i32 p = imul16(absr, r);              // |r| * r
            p = imul16(lo16(p), 20);              // imul bx on the low word only
            dwAdd(M3(pitdotl), M3(pitdoth), frameScale(add32(p, A)));
            dohlock();
            SW(M3(oldzvel), W(M3(topv) + 4));
        }
    }

    // ---- on the ground: wheels, the nose is held by the ground ----
    bool nosedropDone = false;
    if (B(M3(groundflag)) != 0) {
        i16 ax = abs16(W(M3(topv)));
        if (static_cast<u16>(ax) > 2821) {   // control: more than a roll's worth of speed
            ax = neg16(sub16(ax, 2821));
            dwAdd(M3(pitdotl), M3(pitdoth), frameScale(sal32(cwd(ax), 5)));
        } else {                              // stillrol: remove control of the user below this speed
            i16 a = sub16(W(M3(orveceff) + 2), W(M3(pitchmin)));
            const u8 al = static_cast<u8>(a & 0xFF), ah = static_cast<u8>((static_cast<u16>(a) >> 8) & 0xFF);
            // cwd / mov dl,ah / mov ah,al : dx = (sign, ah), ax = al * 257
            const u16 dx = static_cast<u16>(((a < 0 ? 0xFF : 0x00) << 8) | ah);
            const u16 axw = static_cast<u16>((al << 8) | al);
            dwAdd(M3(pitdotl), M3(pitdoth), dwtadj(mk32(axw, dx)));
        }
        // sothere: the nose can't go through the ground; bounce the pitch rate off it
        i16 ax2 = W(M3(orveceff) + 2);
        bool toInair = false;
        auto bounce = [&](i32 v) {
            v = neg32(v);
            v >>= 1;
            dwSet(M3(pitdotl), M3(pitdoth), v);
            ax2 = lo16(v);
        };
        if (ax2 < W(M3(pitchmin))) {                 // cmp ax,pitchmin / jge ckpittloo
            if (W(M3(pitdoth)) < 0) {                 // already moving the right way
                toInair = true;
            } else {
                bounce(add32(dwGet(M3(pitdotl), M3(pitdoth)), frameScale(1000)));
            }
        }
        if (!toInair) {
            if (ax2 < 0) {
                toInair = true;                       // pitch is less than zero
            } else if (W(M3(pitdoth)) >= 0) {
                toInair = true;                       // don't bounce if in the correct direction
            } else {
                bounce(add32(dwGet(M3(pitdotl), M3(pitdoth)), frameScale(-1000)));
            }
        }
        (void)nosedropDone;
    }
    // inair:
    if (B(M3(groundflag)) == 0 && B(M3(qstald)) != 0) {   // stalled: the nose drops
        dwAdd(M3(pitdotl), M3(pitdoth), frameScale(sal32(cwd(W(M3(topv) + 4)), 7)));
    }
    if (B(M3(groundflag)) == 0) {
        const u32 p1 = mul16(static_cast<u16>(W(M3(thstal))), 58000);
        const u32 p2 = mul16(static_cast<u16>(W(M3(rhotop))), static_cast<u16>(p1 >> 16));
        const u16 rho0 = static_cast<u16>(W(M3(rho0)));
        SW(M3(almthstald), static_cast<i16>(rho0 ? p2 / rho0 : 0xFFFF));
    }
    if (B(M3(groundflag)) != 0 || B(M3(qstald)) != 0) {
        SB(M3(stallprotflag), 0);   // nostallprt
    } else {
        i16 ax = add16(W(M3(alpha)), W(M3(alpha)));   // angle of attack doubled
        ax = sub16(ax, W(M3(oldalpha)));              // projected future alpha
        ax = abs16(ax);
        const i16 bx3 = ax;                            // pristine copy
        bool protect;
        if (B(M3(stallprotflag)) != 0) {
            const i16 dx = sub16(abs16(W(M3(pitdoth))), W(M3(mopitdoth)));
            protect = !(dx < 0);                       // jns stallprt
        } else {
            protect = !(static_cast<u16>(ax) < static_cast<u16>(W(M3(almthstald))));
        }
        if (protect) {
            flashstall();
            SB(M3(stallprotflag), 1);
            i32 v = sas::divrs_dw(dwGet(M3(pitdotl), M3(pitdoth)), bx3);
            v = sas::muls_dw(v, W(M3(almthstald)));
            dwSet(M3(pitdotl), M3(pitdoth), v);
            SW(M3(mopitdoth), abs16(hi16(v)));
        } else {
            SB(M3(stallprotflag), 0);
        }
    }

    // ---- stability damage: random jitter of all three rates ----
    if (W(M3(stabdam)) != 0) {
        i16 ax = idiv32(imul16(W(M3(stabdam)), vpxp), 300).q;   // reduce stab damage by airspeed
        ax = add16(abs16(ax), 2);                                 // avoid negative roll over
        const u16 range = static_cast<u16>(ax);
        for (u32 off : {static_cast<u32>(M3(pitdoth)), static_cast<u32>(M3(yawdoth)), static_cast<u32>(M3(thrdoth))}) {
            i16 n = static_cast<i16>(m_rand.randr_w(range));
            n = sub16(n, static_cast<i16>(range >> 1));            // make it symmetric
            SW(off, add16(W(off), static_cast<i16>(frameScale(n))));
        }
    }

    // ---- the rates become a rotation of zmat ----
    // (the rates are 16.16: the whole part used to be scaled by the frame time and the fraction thrown away, which at a
    // short frame turned a slow rate into no turn at all; now the whole dword is scaled and the remainder carried)
    SW(M3(pitdoth), bndck(W(M3(pitdoth)), 3000, -3000));
    const i16 pitchRate = takeWhole(-dwtadjWide(dwGet(M3(pitdotl), M3(pitdoth))), m_angFrac[0]);
    SW(M3(yawdoth), bndck(W(M3(yawdoth)), 3000, -3000));
    const i16 yawRate = takeWhole(dwtadjWide(dwGet(M3(yawdotl), M3(yawdoth))), m_angFrac[1]);
    SW(M3(thrdoth), bndck(W(M3(thrdoth)), 3000, -3000));
    const i16 rollRate = takeWhole(dwtadjWide(dwGet(M3(thrdotl), M3(thrdoth))), m_angFrac[2]);
    m_eng.rolpln(rollRate, pitchRate, yawRate, static_cast<u16>(M3(zmat)));

    // the angles back out of the matrix
    bool shook = false;
    if (B(M3(ejectflag)) == 0) {
        getang(static_cast<u16>(M3(zmat)), static_cast<u16>(M3(orvec)));
        if (B(M3(groundflag)) != 0 && B(M3(onrunwayflag)) == 0) {   // on the grass: shake the plane about
            SW(M3(orvec) + 4, add16(W(M3(orvec) + 4), sub16(static_cast<i16>(m_rand.randr_w(201)), 100)));
            SW(M3(orvec) + 2, add16(W(M3(orvec) + 2), sub16(static_cast<i16>(m_rand.randr_w(241)), 120)));
            shook = true;
        }
    }
    if (!shook) getang(static_cast<u16>(M3(zmat)), static_cast<u16>(M3(obj5d)) + 12);

    // imtired: the flaps change the stall constants; the dumpy plane has less power
    if (B(M3(flapflag)) != 0) {
        SW(M3(thstal), static_cast<i16>(d::m3_equ::efthstal));
        SW(M3(thstl2), static_cast<i16>(d::m3_equ::efthstl2));
    } else {
        SW(M3(thstal), static_cast<i16>(d::m3_equ::ethstal));
        SW(M3(thstl2), static_cast<i16>(d::m3_equ::ethstl2));
    }
    SW(M3(thrtsf), static_cast<i16>(d::m3_equ::ethrtsf));   // assume a nice plane
    SB(M3(mdsclr1), 0x0C);                                  // red air ball
    if (W(M3(obj5dbits)) & 0x1000) {                        // a dumpy plane
        SB(M3(mdsclr1), 0x09);                              // blue air ball
        SW(M3(thrtsf), static_cast<i16>(d::m3_equ::edthrtsf));
    }
}

// ==========================================================================================
// 8796-9141: velocity in the plane's frame, gravity, thrust, drag, lift, and the new velocity
// ==========================================================================================
void Main3::forcesAndVelocity() {
    velmul(static_cast<u16>(M3(rtopv)), static_cast<u16>(M3(nzmat)), static_cast<u16>(M3(topv)));
    for (int k = 0; k < 3; k++) SW(M3(ctopv) + 2 * k, sar16(W(M3(topv) + 2 * k), 4));

    const u16 nz = static_cast<u16>(M3(nzmat));
    const i16 grvsf = W(M3(grvsf));
    SD(M3(dvel), imul16(W(nz + 4), grvsf));         // gravity, in the plane's frame
    SW(M3(sssin), W(nz + 10));                      // save these quantities
    SD(M3(dvel) + 4, imul16(W(nz + 10), grvsf));
    SW(M3(sscos), W(nz + 16));
    SD(M3(dvel) + 8, imul16(W(nz + 16), grvsf));

    {   // count rolls: upside down, and the sign of the matrix element changed
        const i16 m16 = W(nz + 16);
        if (!(static_cast<u16>(abs16(sar16(m16, 1))) < 13000)) {
            if (((m16 ^ W(M3(lastroll))) & 0x8000) != 0) {
                SW(M3(lastroll), m16);
                SW(M3(rollprog), add16(W(M3(rollprog)), 1));
            }
        }
    }

    // throttle multiplier: thin air degrades the engine
    SW(M3(thrtmult), 0);
    {
        const i16 bx = sub16(W(M3(rho0)), W(M3(rho02)));
        i16 ax = sub16(W(M3(rhotop)), bx);
        if (!(ax < 0)) {   // not too high
            SW(M3(thrtmult), -1);
            if (!(static_cast<u16>(ax) > static_cast<u16>(W(M3(rho02))))) {   // ja toohigh
                ax = neg16(ax);
                ax = add16(ax, W(M3(rho02)));
                u32 p = mul16(static_cast<u16>(ax), 20383);
                const u16 rho02 = static_cast<u16>(W(M3(rho02)));
                u16 q = rho02 ? static_cast<u16>(p / rho02) : 0xFFFF;   // scale into -4000 to 16383
                const i16 ang = sub16(static_cast<i16>(q), 4000);
                SW(M3(thrtmult), static_cast<i16>(engine::sinRaw(add16(ang, 16384)).mag));   // cos
            }
        }
    }

    // thrust
    {
        i32 t = static_cast<i32>(mul16(static_cast<u16>(sub16(W(M3(rpm)), 20)), static_cast<u16>(W(M3(thrtsf)))));    // 0 thrust at idle
        t = sas::divrs_dw(t, W(M3(rpmx)));
        i16 thr16 = lo16(t);   // cwd / xchg: only the low word survives, as the high word of the dword
        if (B(M3(rocketflag)) != 0) {   // drain away rockets quickly; their thrust replaces the engine's
            i16 ax = wtadj(5);
            SW(M3(rockettick), sub16(W(M3(rockettick)), ax));
            if (W(M3(rockettick)) < 0) {
                ax = 0;
                SB(M3(rocketflag), 0);
                SW(M3(rockettick), 0);
            }
            thr16 = add16(ax, static_cast<i16>(3 * d::m3_equ::ethrtsf));   // boost engine power by 3
        }
        i32 f = static_cast<i32>(mul16(static_cast<u16>(W(M3(thrtmult))), static_cast<u16>(thr16)));   // degrade at altitude
        i16 bx = W(M3(ctopv));   // check for low velocity
        if (sf16(bx, W(M3(vstal2)))) bx = W(M3(vstal2));
        f = sas::divrs_dw(f, bx);
        f = sas::muls_dw(f, W(M3(nticks)));
        dwAdd(M3(dvel), M3(dvel) + 2, f);
    }

    // drag
    {
        // altitude / 256 into an air pressure variable
        const u32 alt = static_cast<u32>(D(M3(ozeff)));
        i16 ax = static_cast<i16>(((alt >> 8) & 0xFFFF));
        i16 dx = W(M3(rho0z));
        if (ax < 0) ax = 0;
        dx = sub16(dx, ax);
        if (dx < 0) dx = 0;
        const u32 pr = mul16(static_cast<u16>(W(M3(rho0))), static_cast<u16>(dx));
        const u16 rho0z = static_cast<u16>(W(M3(rho0z)));
        const i16 rhotop = static_cast<i16>(rho0z ? pr / rho0z : 0xFFFF);   // air pressure scaled into 0-rho0
        SW(M3(rhotop), rhotop);
        const i16 pdiv16 = sar16(rhotop, 4);
        {
            const i16 v = W(M3(ctopv));
            i16 bx = W(M3(idsf));   // the transonic range of velocity
            if (!sf16(v, W(M3(dvspd)))) bx = W(M3(oidsf));
            SW(M3(ridsf), bx);
            i32 f = imul16(abs16(v), v);   // speed squared, with the sign of the speed
            f = sas::muls_dw(f, pdiv16);
            i16 coeff = W(M3(ridsf));
            if (B(M3(flapflag)) != 0) coeff = sar16(sar16(coeff, 1), 1);   // flaps give more drag
            f = sas::divrs_dw(f, coeff);
            f = sas::muls_dw(f, neg16(W(M3(nticks))));
            dwAdd(M3(dvel), M3(dvel) + 2, f);
        }
        {   // sideways drag
            const i16 v = W(M3(ctopv) + 2);
            i16 bx = W(M3(idsf));
            if (!sf16(v, W(M3(dvspd)))) bx = W(M3(oisdsf));
            SW(M3(risdsf), bx);
            i32 f = imul16(abs16(v), v);
            f = sas::muls_dw(f, pdiv16);
            f = sas::divrs_dw(f, W(M3(risdsf)));
            f = sas::muls_dw(f, neg16(W(M3(nticks))));
            dwAdd(M3(dvel) + 4, M3(dvel) + 6, f);
            // the side force for the weathercock effect: the low word of this frame's delta, or
            // +-infinity if it doesn't fit (".abs_d dx,ax / rcl ax,1": carry = bit 15 of |delta|)
            const u32 ad = f < 0 ? 0u - static_cast<u32>(f) : static_cast<u32>(f);
            i16 sidef = lo16(f);
            if (ad & 0x8000) sidef = static_cast<i16>((hi16(f) < 0 ? -1 : 0) ^ 0x7FFF);
            SW(M3(sidef), sidef);
        }
        if (B(M3(groundflag)) != 0) {   // on the ground there is no sideways motion
            SW(M3(dvel) + 4, 0);
            SW(M3(dvel) + 6, 0);
            SW(M3(topv) + 2, 0);
            SW(M3(sidef), 0);
        }
    }

    // lift
    {
        SW(M3(oldalpha), W(M3(alpha)));
        const i16 alpha = atn2(W(M3(topv)), W(M3(topv) + 4));   // angle of attack
        SW(M3(alpha), alpha);
        i16 bx = W(M3(stlfsf));   // assume stalled
        SB(M3(qstald), 0xFF);
        const i16 ax = add16(alpha, W(M3(thstal)));
        bool stalled = ax < 0 || !sf16(ax, W(M3(thstl2)));
        if (!stalled) {
            bx = W(M3(lfsf));   // the wing is fine
            if (B(M3(flapflag)) != 0) bx = sar16(bx, 1);
            SB(M3(qstald), 0);
            voiceoff(5, 0x200);      // the alarm stops
            drawbutton(0x070e, 3);   // the stall light goes dark
            drawbutton(0x000c, 3);
        } else if (B(M3(groundflag)) == 0) {
            voiceon(5, 0x180);       // stalled in the air: the alarm
            drawbutton(0x0e07, 3);   // and the light comes on
            drawbutton(0x0c00, 3);
        }
        const i16 vz = W(M3(topv) + 4);
        i32 f = imul16(vz, vz);
        if (vz >= 0) f = neg32(f);   // lift opposes the vertical velocity
        f = sas::divrs_dw(f, 16);
        f = sas::divrs_dw(f, bx);
        f = sas::muls_dw(f, W(M3(rhotop)));
        f = sas::muls_dw(f, W(M3(nticks)));
        dwAdd(M3(dvel) + 8, M3(dvel) + 10, f);
    }

    // apply delta-v to topv, then rotate back to the rtopv frame. Only the high word of the
    // frame-time scaled dword is the velocity change.
    {
        i16 dx = takeWhole(dwtadjWide(D(M3(dvel))), m_velFrac[0]);
        SW(M3(topv), add16(W(M3(topv)), dx));
        SW(M3(acctop), dx);   // keep track of the acceleration

        dx = takeWhole(dwtadjWide(D(M3(dvel) + 4)), m_velFrac[1]);
        i16 ax = W(M3(topv) + 2);
        SW(M3(acctop) + 2, dx);
        if (dx == 0 && static_cast<u16>(abs16(ax)) <= 200) ax = 0;   // sideslip fix
        ax = add16(ax, dx);
        SW(M3(topv) + 2, bndck(ax, 9500, -9500));

        const i32 s = dwtadjWide(D(M3(dvel) + 8));
        dx = takeWhole(s, m_velFrac[2]);
        SW(M3(acctop) + 4, dx);
        SW(M3(y1temp), lo16(s));
        SW(M3(topv) + 4, bndck(add16(W(M3(topv) + 4), dx), 9600, -9600));
    }
    velmul(static_cast<u16>(M3(acctop)), static_cast<u16>(M3(nnzmat)), static_cast<u16>(M3(racctop)));
    velmul(static_cast<u16>(M3(topv)), static_cast<u16>(M3(nnzmat)), static_cast<u16>(M3(rtopv)));
}

} // namespace corncob::game
