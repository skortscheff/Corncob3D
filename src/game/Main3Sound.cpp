// Main3Sound.cpp - The sound of the main loop: engine, wind, orb, saucers, KLAs, booms
//
// 3.ASM lines 7640-7792 and 7999-8073, and STRTEXP.ASM's makeboom and bigboom. Voices: 0 engine,
// 1 guns, 2 booms and blams, 3 screech / saucer / orb, 4 KLA, 5 stall alarm.

#include "Main3.hpp"
#include "DGroup.hpp"
#include "core/AsmMath.hpp"
namespace d = corncob::dgroup;

namespace corncob::game {

namespace lbl = corncob::lbl;
using asmx::bndck;

#define M3(name) lbl::name

namespace {
inline i16 add16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) + static_cast<u16>(b))); }
inline i16 sub16(i16 a, i16 b) { return static_cast<i16>(static_cast<u16>(static_cast<u16>(a) - static_cast<u16>(b))); }
} // namespace

// 7641-7727: the orb's whine (when one is close) and the saucers' drone
void Main3::orbAndSaucerSound() {
    if (!m_snd) return;
    u16 ax = UW(M3(orbdist));
    bool checkSauce = false;
    if (B(M3(orbonflag)) == 0) {
        if (ax >= 63) {
            checkSauce = true;   // not close
        } else {                 // an orb is close: start its sound
            voiceoff(3, 0x00FF);
            m_snd->orbinit();
            SB(M3(orbonflag), 0xFF);
            voiceon(3, UW(M3(orbfreq)) & 0x00FF);
            ax = UW(M3(orbdist));
        }
    }
    if (!checkSauce) {
        if (ax < 63) {   // orbonstuff: louder as it comes closer
            i16 v = sub16(static_cast<i16>(ax), 13);
            if (v < 0) v = 0;
            adlwrt(0x4B, static_cast<u8>(v));
            adlwrt(0x48, static_cast<u8>(v));
            adlwrt(0xA3, B(M3(orbfreq)));
            return;
        }
        voiceoff(3, 0x10);   // orbshutdown
        SB(M3(sauceronflag), 0);
        m_snd->screechinit();
        SB(M3(orbonflag), 0);
    }
    if (B(M3(orbonflag)) != 0) return;
    // dosaucerstuff
    ax = UW(M3(saucerdist));
    SW(d::m3::ssdist, static_cast<i16>(ax));
    SB(d::strtexp::saucerflag, 0);
    if (ax < 255) {   // a close saucer
        SB(d::strtexp::saucerflag, 0xFF);
        if (B(M3(sauceronflag)) == 0) {
            voiceoff(3, ax);
            m_snd->saucerinit();
            SB(M3(sauceronflag), 0xFF);
            voiceon(3, 0x00FF);
        }
        ax = static_cast<u16>(ax >> 2);
        SW(M3(ssmag), static_cast<i16>(ax));
        adlwrt(0x4B, static_cast<u8>(ax));
        adlwrt(0xA3, B(M3(ssfreq)));
    } else if (B(M3(sauceronflag)) != 0) {   // scrtofar: it went away
        voiceoff(3, 0x10);
        SB(M3(sauceronflag), 0);
        m_snd->screechinit();
    }
}

// 7729-7792: the KLA missile sites: a tone that glides to its quality and fades in and out
void Main3::klaSound() {
    if (!m_snd) return;
    i16 ax = static_cast<i16>((static_cast<u16>(B(M3(klaqual))) << 4) + 40);   // keep away from zero frequency
    const i16 bx = W(M3(realklafreq));
    ax = sub16(ax, bx);
    ax = static_cast<i16>(ax >> 2);   // the next four frames
    ax = add16(ax, bx);
    SW(M3(realklafreq), ax);
    adlwrt(0xA4, static_cast<u8>(ax));

    u16 loud = 8;   // KLAs get louder, but the minimum attenuation is zero
    if (UW(M3(kladist)) > 8) loud = UW(M3(kladist));
    SW(M3(klaloud), static_cast<i16>(loud));

    if (UW(M3(oldkladist)) < 64) {   // the KLA was on last time
        if (UW(M3(kladist)) >= 64) {
            adlwrt(0x4C, 63);        // turn down to the minimum volume and key off
            voiceoff(4, 63);
        } else {
            adlwrt(0x4C, static_cast<u8>(loud));
        }
    } else if (UW(M3(kladist)) < 64) {   // it should be on now
        adlwrt(0x4C, static_cast<u8>(loud));
        voiceon(4, static_cast<u16>(B(M3(klaqual)) << 5));
    }
    SW(M3(oldkladist), W(M3(kladist)));
}

// 7999-8073: the engine, the wind when on foot
void Main3::engineSound() {
    if (!m_snd) return;
    bool wind = false;
    if (B(M3(remoteflag)) != 0 || B(M3(ejectflag)) == 0) {   // yessound1
        if (W(M3(currpmx)) == 0) {   // the engine is dead
            voiceoff(0, 0x40);
            if (W(M3(crshvel)) == 0) wind = true;
            else return;
        } else {
            const i16 v = W(M3(topv));
            if (v < 0) return;   // skipwind
            u32 num = static_cast<u32>(static_cast<u16>(W(M3(opres)))) + 30u * 40u + static_cast<u32>(v);
            i16 ax = static_cast<i16>(num / 80);
            ax = bndck(ax, 0x400, 0x40);
            // avgthrot: the pitch glides towards its target
            i16 f = static_cast<i16>(static_cast<u16>(ax) << 4);
            const i16 cur = W(M3(throtfrq));
            f = static_cast<i16>(static_cast<i16>(sub16(f, cur)) >> 4);
            f = add16(f, cur);
            SW(M3(throtfrq), f);
            ax = static_cast<i16>(static_cast<u16>(f) >> 4);
            voiceon(0, static_cast<u16>(ax));
            i16 vol = static_cast<i16>(static_cast<u16>(W(M3(engdam))) << 1);
            if (vol < 0) return;   // skipwind
            vol = sub16(vol, 16);
            vol = static_cast<i16>(0 - vol);
            if (vol < 0) vol = 0;
            adlwrt(0x40, static_cast<u8>(vol));   // to make the prop wash constant
            return;
        }
    } else {
        wind = true;   // on foot
    }
    if (wind) {   // nosound1: the wind comes and goes, on a snare
        if (static_cast<i16>(W(M3(windtick)) - static_cast<i16>(m_clock.oticks)) >= 0) return;   // not yet
        SB(M3(windflag), static_cast<u8>(0 - B(M3(windflag))));
        if (static_cast<int8_t>(B(M3(windflag))) >= 0) adlwrt(0xBD, 0x20);
        else adlwrt(0xBD, 0x28);   // turn on the snare, leave the mode alone
        SW(M3(windtick), add16(static_cast<i16>(m_rand.randr_w(4096)), static_cast<i16>(m_clock.oticks)));   // a random time ahead
    }
}

// STRTEXP.ASM makeboom: an explosion; a blam still using the voice has to run down first
void Main3::makeboom() {
    if (!m_snd) return;
    const i16 blam = W(M3(blamtick));
    if (blam != 0) {   // the voice is still a blam
        if (blam == -1) return;
        if (static_cast<u16>(m_clock.oticks - static_cast<u16>(blam)) < 512) return;
        m_snd->b3oominit();
    }
    const u16 now = m_clock.oticks;
    if (static_cast<u16>(now - UW(M3(boomcnt))) < 512) return;
    SW(M3(boomcnt), static_cast<i16>(now));
    voiceoff(2, 0);
    voiceon(2, 0);
}

// F3DVEC.ASM qstopblam, on every page flip: once the screen has stopped shaking the blam of a crash is keyed off and
// the time noted, so that makeboom can turn the voice back into a boom after a while. (Without it the voice sustains
// and, with blamtick left at -1, makeboom never plays an explosion again.)
void Main3::qstopblam() {
    if (!m_snd || B(M3(shakecnt)) != 0 || W(M3(blamtick)) != -1) return;
    voiceoff(2, 0x10);
    SW(M3(blamtick), static_cast<i16>(m_clock.oticks));
}

// STRTEXP.ASM bigboom: the drum for a big explosion
void Main3::bigboom() {
    if (!m_snd) return;
    const u16 now = m_clock.oticks;
    if (static_cast<u16>(now - UW(M3(bboomcnt))) < 512) return;
    SW(M3(bboomcnt), static_cast<i16>(now));
    adlwrt(0xBD, 0x20);
    adlwrt(0xBD, 0x30);
}

// F3DVEC.ASM qcrash: the plane hit something. A blam, or a boom when out of the plane.
void Main3::crashSound(bool boom) {
    if (!m_snd) return;
    if (boom) {
        makeboom();
    } else {
        m_snd->b3laminit();
        voiceon(2, 0x10);
    }
}

// 10484-10497: the S key
void Main3::soundToggle() {
    if (!m_snd) return;
    const u8 now = static_cast<u8>(B(M3(sndflg)) ^ 0xFF);
    if (now == 0) {   // turn it off: quiet the chip first, then stop the writes
        m_snd->soundoff();
        SB(M3(sndflg), 0);
    } else {
        SB(M3(sndflg), now);
        m_snd->soundon();
    }
}

} // namespace corncob::game
