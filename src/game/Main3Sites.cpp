// Main3Sites.cpp - Port of STRTEXP.ASM's enemy sites and the things that fly from them: the anti-aircraft
// guns and their shells, the death balls (and their sites), the vehicles (and their sites), the
// mortars, the walls, the headquarters (the orbs) with the objectives, and initnumbers, which keeps
// count of all of them once a frame
//
// Every object routine takes the object (si), what hit it (di, 0xFFFF = the plane) and whether it is
// being destroyed (carry). The tables of qualities (aaqtbl, dballqtbl ...) are in the data segment:
// the first word says how many words an entry has, the entry for an object is picked by the quality
// bits (12-14) of its type word.

#include "Main3.hpp"
#include "DGroup.hpp"
#include "core/AsmMath.hpp"
#include "engine/AsmTables.hpp"

namespace corncob::game {

namespace d = corncob::dgroup;
namespace lbl = corncob::lbl;
namespace st = corncob::dgroup::strtexp;
using asmx::hmul;
using engine::atn2;
using engine::ssin;

#define M3(name) lbl::name

namespace {

inline i16 add16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) + static_cast<u16>(b))); }
inline i16 sub16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) - static_cast<u16>(b))); }
inline i16 neg16(i16 a) { return static_cast<i16>(static_cast<u16>(0u - static_cast<u16>(a))); }
inline i16 abs16(i16 a) { return a < 0 ? neg16(a) : a; }
inline i32 add32(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b)); }
inline i16 hi16(i32 v) { return static_cast<i16>(static_cast<u32>(v) >> 16); }
inline i16 lo16(i32 v) { return static_cast<i16>(static_cast<u32>(v) & 0xFFFF); }
// the quality of an object, bits 12-14 of its type word
inline u16 quality(u16 type) { return static_cast<u16>((type >> 12) & 7); }
// the limits the ASM puts on a correction: neg(limit) <= ax <= limit, signed
inline i16 limit(i16 ax, i16 lim) {
    const i16 lo = neg16(lim);
    if (!(ax >= lo)) ax = lo;
    if (!(ax <= lim)) ax = lim;
    return ax;
}

} // namespace

// ==========================================================================================
// the helpers
// ==========================================================================================

// getqtblptr: the entry of the quality table at `table` for the object si; the quality is returned
u16 Main3::getqtblptr(u32 table, u16 si, u8& q) {
    const u16 entryBytes = static_cast<u16>(UW(table) * 2);
    q = static_cast<u8>(quality(UW(si + 24)));
    const u16 offset = static_cast<u16>((entryBytes & 0xFF) * q);   // mul bl: only al counts
    const u16 entry = static_cast<u16>(table + 2 + offset);
    SW(st::qtblptr, static_cast<i16>(entry));
    return entry;
}

// enoughits: the hits left drop by what hit us; true = destroyed (the hits are cleared then)
bool Main3::enoughits(u16 si, u16 di) {
    if (di != 0xFFFF) {
        const u8 bl = hitStrength(di);
        const int left = static_cast<int>(B(si + 27)) - bl;
        if (left > 0) {
            SB(si + 27, static_cast<u8>(left));
            return false;   // the object survives
        }
    }
    SB(si + 27, 0);
    return true;
}

// getobsyaw: the yaw that points us to the observer
i16 Main3::getobsyaw() {
    const i16 ax = atn2(neg16(W(M3(dtopv))), neg16(W(M3(dtopv) + 2)));
    return add16(ax, static_cast<i16>(-32768));
}

// getobspit: the pitch that points us to the observer (the height of dtopv is cleared on the way)
i16 Main3::getobspit() {
    const i16 height = W(M3(dtopv) + 4);
    SW(M3(dtopv) + 4, 0);
    const i16 flat = static_cast<i16>(vmag(static_cast<u16>(M3(dtopv))));
    return atn2(flat, neg16(height));
}

// addaa / addball: the better enemies near us add to how well the user is doing
void Main3::addaa(u16 si) {
    const u16 q = quality(UW(si + 24));
    if (q < 4 || B(M3(advflag)) > 0 || B(M3(ejectflag)) != 0) return;
    addToDword(st::aaprog, dwtadj(static_cast<i32>(q * q)));
}

void Main3::addball(u16 si) {
    const u16 q = quality(UW(si + 24));
    if (q < 4 || B(M3(advflag)) > 0 || B(M3(ejectflag)) != 0) return;
    addToDword(st::dballprog, dwtadj(static_cast<i32>(q * q)));
}

// radarprocess: the position of the object relative to the observer; puts a dot on the radar, sets
// dballdist (the distance on the radar plane) and ssdist (its square root), dballvector. true = too far
bool Main3::radarprocess(u16 si) {
    const bool far = B(M3(ejectflag)) != 0 ? m_eng.relobj5d(si) : m_eng.rel3d(si);
    if (far) return true;
    const u16 cx = m_eng.rangeBits();
    if (cx >> 8) return true;
    if (cx & 0xF0) return true;
    addball(si);
    const u32 dv = st::dballvector;
    for (int k = 0; k < 3; k++) {   // the vector to the observer divided by 16, the sign changed
        const u32 v = static_cast<u32>(D(M3(dxvec) + 4 * k)) >> 4;
        SW(dv + 2 * k, neg16(static_cast<i16>(v & 0xFFFF)));
    }
    // the angle: atn2 of the vector's x and y with the sign changed back (the z comes below)
    const i16 x = neg16(W(dv)), y = neg16(W(dv + 2));
    const i16 ang = atn2(x, y);
    SW(dv + 4, 0);   // the magnitude assuming delta-z is zero, for the radar screen
    SW(M3(dballdist), static_cast<i16>(vmag(static_cast<u16>(dv))));
    const u16 dist = engine::dsqrt(UW(M3(dballdist)));
    SW(st::ssdist, static_cast<i16>(dist));
    m_inst.radardot(dist, sub16(ang, W(M3(orveceff))));
    const u32 zv = static_cast<u32>(D(M3(dxvec) + 8)) >> 4;
    SW(dv + 4, neg16(static_cast<i16>(zv & 0xFFFF)));
    return false;
}

// adjdballv: moves the speed of a death ball towards the average at random (di = its quality entry)
void Main3::adjdballv(u16 q) {
    const i16 diff = sub16(W(st::dballavgvel), W(st::dballvel));
    const i16 momentum = W(q + 8);
    i16 ax = momentum != 0 ? static_cast<i16>(diff / momentum) : 0;
    const u16 range = UW(q + 16);   // the randomness factor
    const i16 rnd = sub16(static_cast<i16>(m_rand.randr_w(range)), static_cast<i16>(range >> 1));
    ax = add16(rnd, ax);
    SW(st::dballvel, add16(W(st::dballvel), ax));
}

// ==========================================================================================
// the anti-aircraft guns and the saucers' shells
// ==========================================================================================
void Main3::aatim(u16 si, u16 di, bool carry) {
    if (carry) {
        if (!enoughits(si, di)) return;   // not dead yet
        SB(M3(boomdeadly), 0);            // wimpy
        strtboom(si);
        SW(M3(ondest), add16(W(M3(ondest)), 1));
        const i16 n = sub16(W(st::naa), 1);
        SW(st::naa, n < 0 ? 0 : n);       // one less, never negative
        SB(M3(omniflag), 0);
        strtflt(si, 4);
        makeboom();
        u8 q;
        getqtblptr(st::aaqtbl, si, q);
        incW(M3(naasdest) + 2u * q);
        SW(si + 24, 0);                   // kill us
        return;
    }
    SB(st::saucerflag, 0);
    sauceraa(si);
}

// sauceraa: what an AA gun does each frame (and what a saucer does when it shoots shells): count it,
// turn towards the observer (leading it), and fire at random
void Main3::sauceraa(u16 si) {
    incW(st::curnaas);   // help keep track of the current number of aa's
    u8 qual;
    const u16 q = getqtblptr(st::aaqtbl, si, qual);
    incW(st::tcurnaa + 2u * qual);
    SB(M3(aaclr), static_cast<u8>(UW(q)));   // the default color
    if (getobsvec(si)) {   // aatofar: the observer is too far; stand still
        SW(si + 46, 0);
        SW(si + 48, 0);
        SW(si + 50, 0);
        return;
    }
    if (UW(M3(dballdist)) <= 3000) addaa(si);   // add the aa score to the user's flight prowess

    // lead the observer: how far he moves while the shell is on its way
    if (W(q + 10) != 0) {
        const i16 shellvel = W(st::aashellvel);
        for (int k = 0; k < 3; k++) {
            const i16 bx = static_cast<i16>(W(M3(rtopv) + 2 * k) >> 4);   // diminish the leading by a scale factor
            const i32 prod = static_cast<i32>(W(M3(dballdist))) * bx;   // distance times velocity
            const u16 dx = static_cast<u16>(abs16(hi16(prod)));
            if (static_cast<u16>(dx << 1) >= static_cast<u16>(shellvel)) break;   // can't lead: limited dynamic range
            const i16 lead = static_cast<i16>(prod / shellvel);
            SW(M3(dtopv) + 2 * k, add16(W(M3(dtopv) + 2 * k), lead));
        }
    }

    const bool saucer = B(st::saucerflag) != 0;
    i16 ax = getobspit();   // the pitch to get us to the observer
    if (!saucer) {
        ax = static_cast<i16>(sub16(ax, W(si + 14)) >> 4);   // the difference from the current pitch, as a correction
        ax = limit(ax, W(q + 8));
        ax = wtadj(ax);
    }
    SW(si + 14, add16(W(si + 14), ax));
    ax = getobsyaw();
    if (saucer) {
        SW(si + 12, ax);
    } else {
        ax = static_cast<i16>(sub16(ax, W(si + 12)) >> 4);
        ax = limit(ax, W(q + 6));
        ax = wtadj(ax);
        SW(si + 12, add16(W(si + 12), ax));
    }
    if (W(si + 10) == 0 && UW(si + 8) <= 2000 && !(W(si + 14) < -1200))   // on the ground: don't fire horizontally
        SW(si + 14, -1200);
    m_eng.calcsin(static_cast<u16>(si + 12));
    m_eng.calcmat(static_cast<u16>(M3(dorot)));
    m_eng.ncalcmat(static_cast<u16>(M3(dnrot)));

    if (!(m_rand.rand_w() < UW(q + 2))) return;   // fire at random
    SW(M3(dtopv), W(st::aashellvel));             // the shell's speed, straight ahead
    SW(M3(dtopv) + 2, 0);
    SW(M3(dtopv) + 4, 0);
    u16 shell;
    SW(M3(expobj), static_cast<i16>(M3(fltsm1aashell)));
    if (m_world.expstrt(static_cast<u16>(M3(fltsm1aashell)), shell)) return;
    SW(M3(expobj), static_cast<i16>(shell));
    SB(shell + 18, 0);
    SW(shell + 52, uwtadjRef(static_cast<i16>(d::m3_equ::aashelltime)));   // relative to machine speed
    SW(shell + 24, static_cast<i16>(UW(shell + 24) | 0x8000));
    for (int k = 0; k < 4; k++) SW(shell + 2 * k, W(si + 2 * k));   // the gun's x and y
    const u32 z = static_cast<u32>(UW(si + 8)) + d::m3_equ::dbsz * 4;   // a little above the muzzle
    SW(shell + 8, static_cast<i16>(z & 0xFFFF));
    SW(shell + 10, add16(W(si + 10), static_cast<i16>(z >> 16)));
    m_eng.matvmul(static_cast<u16>(M3(dtopv)), static_cast<u16>(M3(dnrot)), static_cast<u16>(shell + 46));
    SW(shell + 60, W(q + 4));   // the shell's dispersion
}

// aashelltim: a shell explodes when it times out, is hit, or gets close to the plane's height
void Main3::aashelltim(u16 si, bool carry) {
    auto toHell = [&]() {
        SB(M3(boomdeadly), 0x20);   // deadly
        strtboom(si);
        SB(M3(omniflag), 1);
        strtflt(si, 9);
        SW(si + 24, 0);
    };
    if (carry || W(si + 52) == 1) {
        toHell();
        return;
    }
    // how close are we to the plane's height?
    bool notTooLow = true;
    if (W(si + 10) == 0) {
        const i16 half = static_cast<i16>(W(si + 8) >> 1);
        if (!(half < 0) && !(static_cast<u16>(half) > UW(si + 66))) notTooLow = false;   // too low to be blown yet
    }
    if (!notTooLow) return;
    i32 diff = static_cast<i32>(static_cast<u32>(static_cast<u32>(UW(si + 8)) | (static_cast<u32>(UW(si + 10)) << 16)) -
                                static_cast<u32>(D(M3(ozeff))));
    if (diff < 0) diff = -diff;
    if ((static_cast<u32>(diff) >> 16) == 0 && static_cast<u16>(diff) <= 4500) {
        if (!(m_rand.rand_w() > static_cast<u16>(wtadj(3000)))) {   // don't always explode at the correct height
            toHell();
            return;
        }
    }
    SW(si + 66, 6000);   // away: the blast radius is large
    SW(si + 68, 6000);
    SW(si + 70, 6000);
}

// ==========================================================================================
// the death balls (dballs) and their sites
// ==========================================================================================
bool Main3::strtdball(u16 src) {
    u16 neu;
    SW(M3(expobj), static_cast<i16>(M3(fltsm1dball)));
    if (m_world.expstrt(static_cast<u16>(M3(fltsm1dball)), neu)) return true;
    SW(M3(expobj), static_cast<i16>(neu));
    SB(neu + 18, 0);
    SW(neu + 52, static_cast<i16>(d::m3_equ::dballtime));
    u16 q = static_cast<u16>(UW(st::initialqual) & 7);   // merge the quality with the object type
    q = static_cast<u16>((q >> 4) | (q << 12));
    SW(neu + 24, static_cast<i16>(UW(neu + 24) | q | 0x8000));
    SW(neu + 46, 0);
    SW(neu + 48, 0);
    SW(neu + 50, 0);
    SW(neu + 12, 0);
    SW(neu + 16, 0);
    SW(neu + 14, -16384);   // straight up
    for (int k = 0; k < 6; k++) SW(neu + 2 * k, W(src + 2 * k));
    return false;
}

void Main3::dballtim(u16 si, bool carry) {
    if (carry) {
        SB(M3(boomdeadly), 0);   // harmless
        strtboom(si);
        SW(M3(ondest), add16(W(M3(ondest)), 1));
        SB(M3(omniflag), 1);
        strtflt(si, 9);
        u8 q;
        getqtblptr(st::dballqtbl, si, q);
        incW(M3(ndballsdest) + 2u * q);
        SW(si + 24, 0);   // remove from the table
        return;
    }
    SW(st::kladist, 64);
    incW(st::curndballs);
    u8 qual;
    const u16 q = getqtblptr(st::dballqtbl, si, qual);
    incW(st::tcurndballs + 2u * qual);
    SB(M3(dballclr), static_cast<u8>(UW(q)));   // the default color

    if (UW(si + 52) >= d::m3_equ::dballtime2) return;   // not yet active: just sit there, waiting

    SB(si + 64, 0);   // clear gravity and so on
    SW(si + 60, 0);
    SW(si + 62, 0);
    SW(st::dballavgvel, W(q + 12));   // the default target speed
    SB(M3(n), 10);                    // light green for the radar dot
    if (radarprocess(si)) {           // dballtofar: stand still, and queue for termination when old
        SW(si + 46, 0);
        SW(si + 48, 0);
        SW(si + 50, 0);
        if (!(UW(si + 52) > d::m3_equ::dballtime2 / 2)) SW(si + 52, 1);
        return;
    }
    const u16 dist = static_cast<u16>(UW(M3(dballdist)) >> 6);
    if (dist <= UW(st::kladist)) {   // take only the closest kla
        SW(st::kladist, static_cast<i16>(dist));
        SB(st::klaqual, qual);
    }

    // the pitch towards the observer
    const u32 dv = st::dballvector;
    const i16 height = W(dv + 4);
    SW(dv + 4, 0);
    const i16 flat = static_cast<i16>(vmag(static_cast<u16>(dv)));
    i16 ax = atn2(flat, neg16(height));
    ax = static_cast<i16>(sub16(ax, W(si + 14)) >> 4);   // the error, as a correction
    SW(st::dbmaxang, abs16(ax));                          // the largest angle the ball misses by
    ax = wtadj(limit(ax, W(q + 6)));                      // the cornering limit for pitch
    SW(si + 14, add16(W(si + 14), ax));
    // the yaw
    ax = add16(atn2(neg16(W(dv)), neg16(W(dv + 2))), static_cast<i16>(-32768));
    ax = static_cast<i16>(sub16(ax, W(si + 12)) >> 4);
    if (!(static_cast<u16>(abs16(ax)) < UW(st::dbmaxang))) SW(st::dbmaxang, abs16(ax));
    ax = wtadj(limit(ax, W(q + 4)));                      // the cornering limit for yaw
    SW(si + 12, add16(W(si + 12), ax));
    SW(si + 16, add16(W(si + 16), wtadj(W(q + 22))));     // make it roll
    m_eng.calcsin(static_cast<u16>(si + 12));
    m_eng.ncalcmat(static_cast<u16>(M3(dnrot)));
    SW(st::dballvel, static_cast<i16>(vmag(static_cast<u16>(si + 46))));   // the present velocity

    // locked on in both angles: yellow, and up to top speed when far; otherwise slow down
    if (UW(st::dbmaxang) > UW(q + 18)) {
        SW(st::dballavgvel, W(q + 14));
    } else {
        if (m_clock.oticks & 512) SB(M3(dballclr), static_cast<u8>(UW(q + 2)));
        if (UW(M3(dballdist)) >= 200) SW(st::dballavgvel, W(q + 10));   // accelerate to the top speed
        else if (B(st::addprogflag) != 0) SW(st::dballavgvel, W(q + 14));
    }
    adjdballv(q);
    SW(M3(dtopv), W(st::dballvel));   // the forward speed
    SW(M3(dtopv) + 2, 0);
    SW(M3(dtopv) + 4, 0);
    m_eng.matvmul(static_cast<u16>(M3(dtopv)), static_cast<u16>(M3(dnrot)), static_cast<u16>(si + 46));
    // don't let it drag into the ground
    if (W(si + 10) == 0 && !(UW(si + 8) > 1000) && W(si + 50) < 0) SW(si + 50, 0);
}

// the site that makes death balls
void Main3::resetdbst(u16 si) { SW(si + 52, W(st::dbstrest)); }

void Main3::dbsttim(u16 si, u16 di, bool carry) {
    u8 qual;
    if (carry) {
        if (!enoughits(si, di)) {   // dbstsaved
            const u16 q = getqtblptr(st::dbstqtbl, si, qual);
            SB(M3(dbstclr), static_cast<u8>(UW(q)));
            SW(st::dbstrest, W(q + 4));
            resetdbst(si);
            return;
        }
        SB(M3(boomdeadly), 0);
        strtboom(si);
        SW(M3(ondest), add16(W(M3(ondest)), 1));
        SW(M3(nsitesdest), add16(W(M3(nsitesdest)), 1));
        SB(M3(omniflag), 1);
        strtflt(si, 9);
        makeboom();
        SW(si + 24, 0);
        return;
    }
    incW(st::curndbst);
    const u16 q = getqtblptr(st::dbstqtbl, si, qual);
    SB(M3(dbstclr), static_cast<u8>(UW(q)));
    SW(st::dbstrest, W(q + 4));
    if (!(UW(si + 52) < 100)) return;   // not due for another ball
    if (UW(M3(ndballs)) >= UW(st::maxndballs)) {   // too many: don't let the site blow up just for that
        SW(si + 52, add16(W(si + 52), static_cast<i16>(m_refSteps)));
        return;
    }
    resetdbst(si);
    SW(st::initialqual, static_cast<i16>(quality(UW(si + 24))));   // the ball's quality equals ours
    incW(M3(ndballs));
    strtdball(si);
}

// ==========================================================================================
// the vehicles and their sites
// ==========================================================================================
bool Main3::strtvech(u16 src) {
    u16 neu;
    SW(M3(expobj), static_cast<i16>(M3(objvech)));
    if (m_world.expstrt(static_cast<u16>(M3(objvech)), neu)) return true;
    SW(M3(expobj), static_cast<i16>(neu));
    SB(neu + 18, 0);
    SW(neu + 52, uwtadjRef(W(st::vechtime)));   // relative to machine speed
    u16 q = static_cast<u16>(UW(st::initialqual) & 7);
    q = static_cast<u16>((q >> 4) | (q << 12));
    SW(neu + 24, static_cast<i16>(UW(neu + 24) | q | 0x8000));
    SW(neu + 46, 0);
    SW(neu + 48, 0);
    SW(neu + 50, 0);
    SW(neu + 12, 0);
    SW(neu + 14, 0);
    SW(neu + 16, 0);
    for (int k = 0; k < 6; k++) SW(neu + 2 * k, W(src + 2 * k));
    return false;
}

// killvech: overturn the car, and let it sit there a while
void Main3::killvech(u16 si) {
    SW(si + 8, static_cast<i16>(2 * d::m3_equ::veszht));
    SW(si + 16, 20000);   // roll it over
    SW(si + 14, 2400);    // nose it down a little
    SW(si + 52, uwtadjRef(800));
    SB(si + 24, 0x71);    // undestructable, but crashintoable
    SW(si + 10, 0);
    for (int k = 0; k < 3; k++) SW(si + 46 + 2 * k, 0);
    SW(si + 66, 0);       // it destroys nothing again
    SW(si + 68, 0);
    SW(si + 70, 0);
}

// followangles: propels the car forward in the direction of its angles
void Main3::followangles(u16 si) {
    m_eng.calcsin(static_cast<u16>(si + 12));
    m_eng.ncalcmat(static_cast<u16>(M3(dnrot)));
    const u16 q = UW(st::qtblptr);
    u16 ax = UW(q + 4);   // the fast speed, assuming far
    if (B(si + 18) == 0) {   // near: the slow speed
        ax = UW(q + 6);
        if (W(si + 24) & 2) {   // should we slow down?
            ax >>= 2;
            if (W(si + 24) & 4) ax = 0;   // halt if we are stopped
        }
    }
    SW(M3(dtopv), static_cast<i16>(ax));
    SW(M3(dtopv) + 2, 0);
    SW(M3(dtopv) + 4, 0);
    m_eng.matvmul(static_cast<u16>(M3(dtopv)), static_cast<u16>(M3(dnrot)), static_cast<u16>(si + 46));
}

// resetvech: a new direction (towards the observer when he is near enough), or the end of a halted car
void Main3::resetvech(u16 si) {
    const u16 q = UW(st::qtblptr);
    if (!(W(si + 24) & 4)) {   // a vehicle that isn't stopped turns to a random direction
        SW(si + 14, 0);
        SW(si + 16, 0);
        SW(si + 12, static_cast<i16>(m_rand.rand_w()));
    }
    u16 dx = UW(q + 10);   // the randomness assuming we are close
    const u8 dist = B(si + 18);
    if (dist == 3) {   // pretty far: remove it without fanfare
        SW(si + 24, 0);
        return;
    }
    bool track;
    if (dist == 0) {
        track = true;   // stupendously close: definitely track
    } else {
        if (dist != 1) dx = UW(q + 8);   // the randomness assuming we are far
        track = m_rand.rand_w() <= dx;
    }
    auto notrak = [&]() {
        SW(si + 24, static_cast<i16>(UW(si + 24) & ~6));   // clear the halt bits
        followangles(si);
    };
    if (!track) {
        notrak();
        return;
    }
    bool far;
    if (B(M3(ejectflag)) != 0) {   // out of the plane: temporarily fool the observer
        const u8 saved = B(M3(ejectflag));
        SB(M3(ejectflag), 0);
        far = getobsvec(si);
        SB(M3(ejectflag), saved);
    } else {
        far = getobsvec(si);
    }
    if (far) {
        notrak();
        return;
    }
    if (!(W(si + 24) & 4)) SW(si + 12, getobsyaw());   // aim at him, unless halted (then no jerking)
    if (UW(M3(dballdist)) > 256 || B(M3(ejectflag)) == 0) {   // not close, or in the plane
        notrak();
        return;
    }
    if (B(st::haltflag) == 0) {
        SB(st::haltflag, 0xFF);
        SW(si + 24, static_cast<i16>(UW(si + 24) | 2));   // the bit that we are close
    }
    if (UW(M3(dballdist)) <= 100) {   // he's about to be captured
        SB(st::haltflag, 1);
        SW(si + 24, static_cast<i16>(UW(si + 24) | 4));   // way close
        if (B(si + 19) == 1) {   // the rescue van
            SB(st::haltflag, 0);   // stop the capture
            if (UW(M3(dballdist)) <= 32) SB(st::rescueflag, 0xFF);
        }
    }
    for (int k = 0; k < 6; k++) SW(st::haltox + 2 * k, W(si + 2 * k));   // the coordinates for the halt
}

void Main3::vechtim(u16 si, u16 di, bool carry) {
    u8 qual;
    u16 q = getqtblptr(st::vechqtbl, si, qual);   // always get the colors set correctly
    SB(M3(bodclr), static_cast<u8>(UW(q)));
    SB(M3(whlclr), static_cast<u8>(UW(q + 2)));
    if ((UW(si + 24) & 0xFF) == 0x71) {   // exploded: sits a while, then goes
        if (W(si + 52) == 0) SW(si + 24, 0);
        return;
    }
    SW(si + 8, 0);   // vehicles stay on the ground
    SW(si + 10, 0);
    if (carry) {
        bool regular = true;
        if (W(si + 24) & 0x800) {   // special: only bullets, missiles and booms hurt
            const u16 t = UW(di + 72);
            regular = t == 30 || t == 21 || t == 19;
        }
        if (regular && enoughits(si, di)) {
            SB(si + 27, 0xFF);   // a carcass
            SB(M3(boomdeadly), 0);
            strtboom(si);
            SW(M3(ondest), add16(W(M3(ondest)), 1));
            getqtblptr(st::vechqtbl, si, qual);
            incW(M3(nvechsdest) + 2u * qual);
            SB(M3(omniflag), 1);
            strtflt(si, 9);
            killvech(si);
            u16 t = UW(si + 52);
            if (t != 0) SW(si + 52, static_cast<i16>((t >> 2) + 300));   // the lifetime of the wreck
            return;
        }
        q = getqtblptr(st::vechqtbl, si, qual);   // vechsaved
        resetvech(si);
        return;
    }
    if (B(si + 19) == 1) incW(st::curnrvs);   // a friendly van
    SB(M3(vechantclr), 0);
    if (W(si + 24) & 0x800) {   // special: can't die, the antenna flashes
        if (m_clock.oticks & 512) SB(M3(vechantclr), 15);
        SW(si + 52, add16(W(si + 52), static_cast<i16>(m_refSteps)));
    }
    incW(st::curnvechs);
    q = UW(st::qtblptr);
    u16 bx = static_cast<u16>(wtadj(W(q + 12)));   // the reset probability, relative to machine speed
    bool reset;
    if (B(si + 18) == 0) {   // stupendously close
        if (W(si + 24) & 4) {   // stopped: definitely track
            reset = true;
        } else {
            bx = static_cast<u16>(bx << 2);   // four times more likely to change direction
            reset = !(m_rand.rand_w() > bx);
        }
    } else {
        reset = !(m_rand.rand_w() > bx);
    }
    if (reset) resetvech(si);
    if (W(si + 24) & 4) {   // stopped
        SW(si + 46, 0);
        SW(si + 48, 0);
        SW(si + 50, 0);
        if (B(si + 19) != 1) followangles(si);
        return;
    }
    q = UW(st::qtblptr);
    // a serpentine path: a six second cycle in position
    const u32 prod = static_cast<u32>(m_clock.oticks) * UW(q + 14);
    const i16 s = ssin(static_cast<i16>(prod & 0xFFFF));
    const i16 divisor = W(q + 16);
    SW(si + 12, add16(W(si + 12), divisor != 0 ? static_cast<i16>(s / divisor) : 0));
    followangles(si);
}

void Main3::resetvest(u16 si) {
    u16 ax = static_cast<u16>(UW(st::vechrest) * 2);   // twice the rest time and a little more
    ax = static_cast<u16>(ax + (ax >> 1));
    SW(si + 52, static_cast<i16>(ax));
}

void Main3::vesttim(u16 si, u16 di, bool carry) {
    u8 qual;
    if (carry) {
        if (!enoughits(si, di)) {
            getqtblptr(st::vestqtbl, si, qual);   // vestsaved
            resetvest(si);
            return;
        }
        SB(M3(boomdeadly), 0);
        strtboom(si);
        SW(M3(ondest), add16(W(M3(ondest)), 1));
        SB(M3(omniflag), 1);
        strtflt(si, 9);
        makeboom();
        getqtblptr(st::vestqtbl, si, qual);
        incW(M3(nsitesdest) + 2u * qual);
        SW(si + 24, 0);
        return;
    }
    incW(st::curnvest);
    const u16 q = getqtblptr(st::vestqtbl, si, qual);
    SB(M3(bodclr), static_cast<u8>(UW(q)));
    SW(st::vechrest, W(q + 2));   // the time for existence
    if (!(UW(si + 52) < UW(st::vechrest))) return;   // not due for another vehicle
    resetvest(si);
    u16 bx = UW(st::maxnvechs);
    if (bx < 4) bx = 4;   // always allow at least 4 of them
    if (UW(st::nvechs) >= bx) return;   // too many
    SW(st::initialqual, static_cast<i16>(quality(UW(si + 24))));
    incW(st::nvechs);
    strtvech(si);
    const u8 cls = static_cast<u8>(neg16(add16(W(st::nvechs), 1)) & 0xFF);   // a unique class for both
    SB(static_cast<u16>(W(M3(expobj))) + 19, cls);
    SB(si + 19, cls);
}

// ==========================================================================================
// barrage balloons, mortars, walls
// ==========================================================================================
void Main3::bbsmtim(u16 si, bool carry) {
    SW(si + 12, static_cast<i16>(m_rand.rand_w()));
    SW(si + 14, static_cast<i16>(m_rand.rand_w()));
    SW(si + 16, static_cast<i16>(m_rand.rand_w()));
    bbtim(si, carry);
}

void Main3::bbtim(u16 si, bool carry) {
    SW(si + 52, 32000);
    if (carry) return;
    u8 qual;
    const u16 q = getqtblptr(st::bbqtbl, si, qual);
    SB(M3(bbclr), static_cast<u8>(UW(q)));
    auto up = [&]() { SW(si + 50, wtadj(W(q + 2))); };
    if (W(si + 50) == 0) { up(); return; }
    const i16 dx = W(si + 10);
    if (dx < 0) { up(); return; }
    if (dx == 0) {   // chklower
        if (UW(si + 8) < d::m3_equ::bbsize) { up(); return; }
        const u16 chance = static_cast<u16>(wtadj(W(q + 4)));
        if (!(m_rand.rand_w() > chance)) SW(si + 50, neg16(W(si + 50)));   // change direction
        return;
    }
    SW(si + 50, neg16(wtadj(W(q + 2))));   // headdown
}

void Main3::mortim(u16 si, u16 di, bool carry) {
    if (carry) {
        if (!enoughits(si, di)) return;
        SB(M3(boomdeadly), 0);
        strtboom(si);
        SW(M3(ondest), add16(W(M3(ondest)), 1));
        SW(M3(nsitesdest), add16(W(M3(nsitesdest)), 1));
        SB(M3(omniflag), 1);
        strtflt(si, 9);
        SW(si + 24, 0);
        return;
    }
    constexpr int recharged = d::m3_equ::morttime - d::m3_equ::mortrechargetime;
    if (UW(si + 52) > recharged) return;
    SW(si + 52, recharged);
    if (B(si + 18) != 0) return;
    // how far is the plane: |dx| + |dy| against the firing distance
    auto absDiff = [&](u32 off, u32 ref) -> u32 {
        const i32 v = static_cast<i32>(static_cast<u32>(D(off)) - static_cast<u32>(D(ref)));
        return static_cast<u32>(v < 0 ? -v : v);
    };
    const u32 dist = absDiff(si, M3(oxeff)) + absDiff(si + 4, M3(oxeff) + 4);
    if (dist > static_cast<u32>(D(st::mortfiredist))) return;
    SW(si + 52, d::m3_equ::morttime);
    SB(M3(flttype), 3);   // flag that we are shooting a mortar
    SB(M3(omniflag), 0);  // make it spew up
    strtflt(si, 2);
    SB(M3(flttype), 0);
}

void Main3::walltim(u16 si, bool carry) {
    if (carry) {
        SW(si + 44, W(si + 40));
        SW(si + 52, 32000);
        return;
    }
    u8 qual;
    const u16 q = getqtblptr(st::wallqtbl, si, qual);
    SB(M3(wallc), static_cast<u8>(UW(q)));
    if (W(st::curndeadhqs) != 0) {   // wallshutdown
        if (W(st::nhqs) == 0) SW(si + 24, 0);
        return;
    }
    if (qual != 0) return;   // not a flashing one: make it solid
    if (UW(si + 52) > 31992) return;
    SW(si + 52, 31990);
    SW(si + 44, static_cast<i16>(M3(nulobj)));
}

void Main3::lwalltim(u16 si, bool carry) {
    if (carry) return;
    u8 qual;
    const u16 q = getqtblptr(st::wallqtbl, si, qual);
    u8 color = static_cast<u8>(UW(q));
    if (W(q) < 0) color = B(M3(clrhq));   // -1: use the headquarters color
    SB(M3(lwallclr), color);
    if (W(st::curndeadhqs) != 0) {   // lwallshutdown
        if (W(st::nhqs) == 0) SW(si + 24, 0);
        return;
    }
    SW(si + 52, 32000);
}

// ==========================================================================================
// the headquarters (the orbs) and the mission objectives
// ==========================================================================================

// incondest: one more objective reached; the first one completes the mission
void Main3::incondest(u16 si) {
    if (W(si + 24) & 0x200) return;   // the objective has been done
    SW(M3(nhqsdest), add16(W(M3(nhqsdest)), 1));
    SW(si + 24, static_cast<i16>(UW(si + 24) | 0x200));   // lock out further increments
    if (B(M3(cmpltflag)) == 0) SB(M3(cmpltflag), 0xFF);   // the mission complete flag
    SW(M3(rmsgqueue), static_cast<i16>(M3(cmpltrmsg)));
    if (!(UW(M3(totaldamage)) < 16)) SW(M3(rmsgqueue), static_cast<i16>(M3(cmpltdmsg)));
}

void Main3::randspin(u16 si) {
    SW(si + 54, static_cast<i16>(m_rand.randr_w(60) + 20));
    SW(si + 56, static_cast<i16>(m_rand.randr_w(40) + 10));
    SW(si + 58, static_cast<i16>(m_rand.randr_w(110) + 30));
}

// attractplane: a gravity orb pulls the plane (and not the man on foot) towards it
void Main3::attractplane(u16 si) {
    (void)si;
    if (B(M3(ejectflag)) != 0) return;
    getobsvec(si);
    const u16 dist = UW(M3(dballdist));
    if (dist > 5000) return;
    u32 ax = static_cast<u32>(dist) * dist / 5000;   // the distance squared, over 5000
    if (!(ax > 10)) ax = 10;
    const i32 bx = static_cast<i32>(ax);
    for (int k = 0; k < 3; k++) {
        const i32 v = (D(M3(dxvec) + 4 * k) >> 3) / bx;
        SW(M3(rtopv) + 2 * k, add16(W(M3(rtopv) + 2 * k), wtadj(static_cast<i16>(v & 0xFFFF))));
    }
}

// the kill routines of the quality table hqtbl: what makes the mission complete
static constexpr int HQ_KILLME = 0, HQ_TICKS = 1, HQ_DBAA = 2, HQ_VECHS = 3, HQ_TOUGH = 4, HQ_BMRST = 5, HQ_STETS = 6, HQ_AL = 7;

void Main3::hqtim(u16 si) {
    incW(st::curnhqs);
    SB(M3(tripflag), static_cast<u8>(B(M3(tripflag)) & ~2));
    SW(st::tripamount, 0);
    SW(st::orbdist, 63);   // assume no orb around
    bool trip = false;
    if (!getobsvec(si, true)) {   // we want the player, not the plane
        const u16 dist = UW(M3(dballdist));
        if (dist <= 2000) {
            SB(M3(tripflag), static_cast<u8>(B(M3(tripflag)) | 2));
            constexpr int hqgrav = 70;
            u16 ax = static_cast<u16>(dist * hqgrav / 2000);   // now 0-75
            const u16 saved = ax;
            ax = static_cast<u16>(hqgrav + 1 - ax);
            if (ax > hqgrav / 2) ax = hqgrav / 2;   // curb the trip a little
            SW(st::tripamount, static_cast<i16>(ax));
            ax = saved > 63 ? 63 : saved;   // this will be a loudness
            SW(st::orbdist, static_cast<i16>(ax));
            trip = (W(si + 24) & 0x800) != 0;
        }
    }
    if (trip) attractplane(si);
    hqtim1(si);
}

// hqtim1: the colors of the orb, and the code that checks its objective
void Main3::hqtim1(u16 si) {
    SW(si + 52, 32000);   // the object lives forever
    u8 qual;
    const u16 q = getqtblptr(st::hqtbl, si, qual);
    SB(M3(clrhqb), static_cast<u8>(UW(q)));
    SB(M3(clrhqc), 0);
    if ((W(si + 24) & 0x800) && (m_clock.oticks & 512)) SB(M3(clrhqc), 15);
    if ((W(si + 24) & 0x200) && !(m_clock.oticks & (512 + 256))) SB(M3(clrhqb), static_cast<u8>(B(M3(clrhqb)) ^ 0x08));
    if (B(si + 18) != 0 && static_cast<u32>(D(si + 20)) > static_cast<u32>(D(si + 32))) return;   // only run the code when near
    switch (qual) {
    case HQ_KILLME:
        if (W(si + 72) != 0) incondest(si);   // still an orb? it lights up when all hqs are dead
        break;
    case HQ_TICKS:
        if (static_cast<u16>(UW(st::ntets) + UW(st::maxnticks)) == 0) incondest(si);   // all ticks and tets dead
        break;
    case HQ_DBAA:
        if (static_cast<u16>(UW(st::maxndballs) + UW(st::naas)) == 0) incondest(si);
        break;
    case HQ_VECHS:
        if (static_cast<u16>(UW(st::maxnvechs) + UW(st::nvechs)) == 0) incondest(si);
        break;
    case HQ_TOUGH:   // killmetough: only a plane can take us
        SB(si + 27, d::m3_equ::hqhits);   // fully regenerate any damage
        if (W(si + 72) != 0) {
            const u16 status = static_cast<u16>(UW(M3(missionstatus)) | 0x8000);   // a suicide mission succeeded
            SW(M3(missionstatus), static_cast<i16>(status));
            if (!(status & 0x40)) SW(M3(patchcrash), static_cast<i16>(UW(M3(patchcrash)) & ~0x40));
            incW(st::nkillmetough);
            incondest(si);
        }
        break;
    case HQ_BMRST:
        if (static_cast<u16>(UW(st::nbmrst) + UW(st::nbmrs)) == 0) incondest(si);
        break;
    case HQ_STETS:
        if (UW(st::nstets) == 0) incondest(si);
        break;
    case HQ_AL:
        if (UW(st::maxnals) != 0 && UW(st::nals) == 0) incondest(si);
        break;
    }
}

// dhqtim: the wreck of a headquarters; when the mission is complete it falls to the ground
void Main3::dhqtim(u16 si) {
    SW(si + 52, 32000);   // lives forever
    incW(st::ndeadhqs);
    if (W(si + 24) & 0x200) {   // the mission is completed
        if (W(si + 10) != 0 || UW(si + 8) > static_cast<u16>(d::m3_equ::lhq * 4)) {   // yesdrop
            randspin(si);
            SB(si + 64, 35);   // make it fall
        } else {
            SW(si + 10, 0);
            SW(si + 8, static_cast<i16>(d::m3_equ::xgmx));
            for (int k = 0; k < 3; k++) SW(si + 46 + 2 * k, 0);
            SW(si + 60, 0);
            SW(si + 62, 0);
        }
    }
    SB(M3(tripflag), static_cast<u8>(B(M3(tripflag)) & ~2));
    hqtim1(si);
}

// bigdest: the headquarters orb (object type 0)
void Main3::bigdest(u16 si, u16 di, bool carry) {
    if (!carry) {
        hqtim(si);
        return;
    }
    bool destroy = false;
    if (di == 0xFFFF) {   // the plane has hit: kills the pilot if it is a killme-tough orb
        if ((UW(si + 24) & 0x7000) == 0x4000 && B(M3(ejectflag)) == 0) {
            SB(M3(crshflg), 11);   // a fatal crash with the building
            SW(M3(missionstatus), static_cast<i16>(UW(M3(missionstatus)) | 0x1000));
        }
        destroy = true;
    } else {
        if ((UW(si + 24) & 0x7000) == 0x4000) return;   // quality 5 is killme tough: no shards even
        const u8 bl = hitStrength(di);
        const int left = static_cast<int>(B(si + 27)) - bl;
        if (left > 0) {
            SB(si + 27, static_cast<u8>(left));
            if (bl >= 6) {   // a single hit doesn't let anything fly
                SB(M3(omniflag), 0);
                strtshard(si, 2);
            }
            return;
        }
        destroy = true;
    }
    if (!destroy) return;
    SB(si + 27, 0);   // clear the hits so they can't go negative
    const i16 wreck = static_cast<i16>(M3(dhqobj));
    SW(si + 40, wreck);
    SW(si + 42, wreck);
    SW(si + 44, wreck);
    SW(si + 24, static_cast<i16>((UW(si + 24) & 0xFF00) | 0x11));   // non-destructable, keeping the rest of the bits
    SW(si + 72, 43);   // make it into wreckage
    SW(M3(ondest), add16(W(M3(ondest)), 1));
    SB(M3(omniflag), 0);
    strtflt(si, 12);
    strtshard(si, 10);
    bigboom();
    u8 qual;
    getqtblptr(st::hqtbl, si, qual);
    incW(M3(norbsdest) + 2u * qual);
}

// ==========================================================================================
// initnumbers: once a frame, the counts of this frame become the counts the next frame sees
// ==========================================================================================
void Main3::initnumbers() {
    SW(st::kladist, 64);
    SW(st::orbdist, 63);   // the default is no orb
    SB(st::orbfreq, 128);
    SW(st::saucerdist, 256);
    if (B(st::addprogflag) != 0) SB(st::addprogflag, 0);   // if we kept score last frame, skip it here
    if (static_cast<u16>(m_clock.oticks - UW(st::lasttick)) >= 2048) SB(st::addprogflag, 0xFF);   // add progress to the charts

    // the score capability of the current number of aa guns and death balls
    u16 newaasum = 0, newdballsum = 0;
    for (int k = 0; k < 8; k++) {
        newdballsum = static_cast<u16>(newdballsum + UW(st::tcurndballs + 2 * k) * (k + 1));
        newaasum = static_cast<u16>(newaasum + UW(st::tcurnaa + 2 * k) * (k + 1));
    }
    SW(st::newaasum, static_cast<i16>(newaasum));
    SW(st::newdballsum, static_cast<i16>(newdballsum));
    for (int k = 0; k < 8; k++) {
        if (newdballsum > UW(st::dballsum)) SW(M3(maxdbonme) + 2 * k, W(st::tcurndballs + 2 * k));
        SW(st::tcurndballs + 2 * k, 0);
        if (newaasum > UW(st::aasum)) SW(M3(maxaaonme) + 2 * k, W(st::tcurnaa + 2 * k));
        SW(st::tcurnaa + 2 * k, 0);
    }
    if (UW(st::curnals) > UW(st::maxnals)) SW(st::maxnals, W(st::curnals));
    if (newaasum > UW(st::aasum)) SW(st::aasum, static_cast<i16>(newaasum));   // only a true maximum
    if (newdballsum > UW(st::dballsum)) SW(st::dballsum, static_cast<i16>(newdballsum));

    SW(st::maxnrvs, W(st::curnrvs));
    SW(st::maxnticks, W(st::curnticks));
    SW(st::maxndballs, W(st::curndbst));
    SW(st::maxnvechs, W(st::curnvest));
    SW(st::nvechs, W(st::curnvechs));
    SW(M3(ndballs), W(st::curndballs));
    SW(st::curndeadhqs, W(st::ndeadhqs));
    SW(st::nbmrs, W(st::curnbmrs));
    SW(st::nbmrst, W(st::curnbmrst));
    SW(st::nals, W(st::curnals));
    SW(st::nstets, W(st::curnstets));
    SW(st::ntets, W(st::curntets));
    SW(st::naas, W(st::curnaas));
    SW(st::nhqs, W(st::curnhqs));
    for (u32 v : {st::curnhqs, st::curnaas, st::curnstets, st::curntets, st::curnals, st::curndbst, st::curnvest,
                  st::curndballs, st::curnvechs, st::curnticks, st::curnrvs, st::curnbmrs, st::curnbmrst, st::ndeadhqs})
        SW(v, 0);
    SB(st::haltflag, 0);
    changewallobjs();
}

} // namespace corncob::game
