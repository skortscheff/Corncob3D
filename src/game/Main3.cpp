// Main3.cpp - Port of the 3.ASM main loop, first half: set-up, timing, views and drawing the world
//
// Line numbers in the comments are 3.ASM's. See Main3.hpp for the conventions.

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
using asmx::sf16;
using engine::atn2;
using engine::scos;
using engine::ssin;
namespace lbl = corncob::lbl;

const u32 Main3::SCNCODE = d::m3::scncode;

namespace {

// 3.ASM labels used in this file
#define M3(name) lbl::name

inline i16 add16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) + static_cast<u16>(b))); }
inline i16 sub16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) - static_cast<u16>(b))); }
inline i16 neg16(i16 a) { return static_cast<i16>(static_cast<u16>(0u - static_cast<u16>(a))); }
inline i16 abs16(i16 a) { return a < 0 ? neg16(a) : a; }
inline u32 abs32(i32 v) { return v < 0 ? 0u - static_cast<u32>(v) : static_cast<u32>(v); }

} // namespace

Main3::Main3(GameState& gs, engine::F3dVec& eng, World& world, DosKeyboard& kbd, TickClock& clock,
             Instruments& inst)
    : m_g(gs), m_eng(eng), m_world(world), m_kbd(kbd), m_clock(clock), m_inst(inst) {
    m_eng.setHooks(this);
    m_eng.setTickSource(&m_clock.oticks);
}

// ---- wtadj / uwtadj / dwtadj (3DMAC.INC) ----
i16 Main3::wtadj(i16 v) const { return asmx::wtadj(v, W(M3(ofrmticks)), W(M3(ticksf))); }
i16 Main3::uwtadj(i16 v) const { return asmx::uwtadj(v, W(M3(ofrmticks)), W(M3(ticksf))); }
i32 Main3::dwtadj(i32 v) const {
    return sas::divrs_dw(sas::muls_dw(v, W(M3(ofrmticks))), W(M3(ticksf)));
}

// ==========================================================================================
// 3.ASM: the start of the loop (7060-7234)
// ==========================================================================================

void Main3::timing() {
    SW(M3(nframes), add16(W(M3(nframes)), 1));
    u16 ax = m_clock.frmticks;
    if (W(M3(qtimeinc)) != 0) {   // forced mode: pretend oticks is running
        ax = static_cast<u16>(W(M3(qtimeinc)));
        m_clock.oticks = static_cast<u16>(m_clock.oticks + ax);
    }
    ax >>= 3;
    const i16 t = bndck(static_cast<i16>(ax), W(M3(maxofrm)), W(M3(minofrm)));   // no zero, no huge frames
    SB(M3(advflag), 0);
    SW(M3(ofrmticks), t);
    SW(M3(realofrmticks), t);
    if (key(0x14)) {   // 't': accelerated time
        SW(M3(ofrmticks), 50);
        SB(M3(advflag), 0xFF);
    }
}

// 7137-7233: pause and help. The original blocks in a loop until a second 'p' (or escape); here
// frame() idles until then. F4 (or a help request) starts the same loop with a space pushed, so
// the first help screen shows at once; space steps through the screens, the hold button flashes.
bool Main3::pauseAndHelp() {
    const bool helpReq = B(M3(helpreqflag)) != 0 || scncode() == 0x3E + 0x80;
    if (!m_paused) {
        if (helpReq) SB(SCNCODE, 0x39 + 0x80);   // push a space key into the buffer: pretend the pause key
        else if (scncode() != 0x19 + 0x80) return false;
        if (key(0x38)) return false;   // alt is down: not a pause
        if (B(M3(helpreqflag)) == 0 && scncode() != 0x39 + 0x80) clearScn();
        SB(M3(helpreqflag), 0);
        SB(M3(curhelpindex), static_cast<u8>(d::m3_equ::mincurhelpindex));
        if (B(M3(groundflag)) == 0) SB(M3(curhelpindex), static_cast<u8>(d::m3_equ::mincurhelpindex + 1));   // skip the intro
        voiceoff(0, 0x100);      // the engine and the blams fall silent
        voiceoff(2, 0x10);
        drawbutton(0x0f07, 1);   // turn on the hold button
        drawbutton(0x0500, 1);
        m_holdColors = 0x070f;
        m_paused = true;
    }
    if (scncode() == 0x39 + 0x80) {   // space: the next help screen
        clearScn();
        const u8 image = B(M3(helpimgtable) + B(M3(curhelpindex)));
        if (m_showImage) m_showImage(image);
        SB(M3(curhelpindex), static_cast<u8>(B(M3(curhelpindex)) + 1));
        if (B(M3(curhelpindex)) > d::m3_equ::maxcurhelpindex) SB(M3(curhelpindex), static_cast<u8>(d::m3_equ::mincurhelpindex));
    }
    if (!(m_clock.oticks & 1023)) m_holdColors = static_cast<u16>((m_holdColors << 8) | (m_holdColors >> 8));   // make the button flash
    drawbutton(m_holdColors, 1);
    if (scncode() == 0x19 + 0x80 || scncode() == 0x81) {   // p again, or escape: back to the game
        clearScn();
        m_paused = false;
        drawbutton(0x0005, 1);   // turn off the background of the button
        if ((m_holdColors >> 8) == 0x0f) drawbutton(static_cast<u16>((m_holdColors << 8) | (m_holdColors >> 8)), 1);
        restoreview();           // the help screens covered the instrument panel
        return false;
    }
    return true;
}
// ==========================================================================================
// 7324-7403: the arrow pointing to the home airport
// ==========================================================================================

void Main3::homeArrow() {
    for (int k = 0; k < 6; k++) SW(M3(arrowbuf) + 2 * k, W(M3(startcoords) + 2 * k));
    const i32 dx = static_cast<i32>(static_cast<u32>(D(M3(arrowbuf))) - static_cast<u32>(D(M3(ox))));
    const i32 dy = static_cast<i32>(static_cast<u32>(D(M3(arrowbuf) + 4)) - static_cast<u32>(D(M3(ox) + 4)));
    const i16 ax = static_cast<i16>(static_cast<u32>(static_cast<u32>(dx) << 4) >> 16);
    const i16 dyh = static_cast<i16>(static_cast<u32>(static_cast<u32>(dy) << 4) >> 16);
    const u16 mag = static_cast<u16>(abs16(ax) + abs16(dyh));
    SB(M3(arrowflag), mag > 10 ? 1 : 0);
    i16 angle = atn2(ax, dyh);
    angle = sub16(angle, W(M3(orvec)));
    SW(M3(homeangle), angle);
    if (!B(M3(arrowflag))) return;
    const u16 win = UW(M3(ptrgwinf));
    i16 y = add16(W(win + 6), 2);   // wymax of the window
    SW(M3(argy2), y);
    SW(M3(argy1), add16(y, 6));
    i16 a = neg16(static_cast<i16>(W(M3(homeangle)) >> 1));
    a = bndck(a, 4000, -4000);
    const asmx::DivResult r = asmx::idiv32(asmx::imul16(a, 310), 4000);
    SW(M3(argx1), add16(r.q, 310));
    SW(M3(argx2), add16(W(M3(argx1)), 2));
    SB(M3(n), 0);
    m_eng.kline();
    SW(M3(argx1), add16(W(M3(argx1)), 4));
    m_eng.kline();
}

// ==========================================================================================
// 7459-7516: view direction (looking around with the cursor keys while the control key is down)
// ==========================================================================================

void Main3::viewMatrices() {
    i16 bx = uwtadj(2);   // relativise the look speed to the machine speed
    if (bx < 2) bx = 2;
    for (int k = 0; k < 2; k++) {
        const i16 diff = sub16(W(M3(desiredyaw) + 2 * k), W(M3(curryaw) + 2 * k));
        if (diff != 0) SW(M3(curryaw) + 2 * k, add16(W(M3(curryaw) + 2 * k), static_cast<i16>(diff / bx)));
    }
    if (B(M3(desireflag)) != 0) {
        for (int k = 0; k < 9; k++) SW(M3(curnmat) + 2 * k, W(lbl::norot + 2 * k));
        m_eng.rolpln(W(M3(curryaw) + 4), W(M3(curryaw) + 2), W(M3(curryaw)), static_cast<u16>(M3(curnmat)));
        getang(static_cast<u16>(M3(curnmat)), static_cast<u16>(M3(obsangs)));
        m_eng.calcsin(static_cast<u16>(M3(obsangs)));
        m_eng.calcmat(static_cast<u16>(M3(curmat)));
        SW(M3(orotptr), static_cast<i16>(M3(curmat)));
        SW(M3(norotptr), static_cast<i16>(M3(curnmat)));
    } else {
        SW(M3(viewkludge), W(M3(orvec) + 2));
        SW(M3(orotptr), static_cast<i16>(lbl::orot));
        SW(M3(norotptr), static_cast<i16>(lbl::norot));
    }
}

} // namespace corncob::game
