// Sound.cpp - Port of ADL.ASM (and soundoff/soundon/b3laminit of 3.ASM)

#include "Sound.hpp"
#include "DGroup.hpp"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace corncob {

namespace lbl = corncob::lbl;

void Sound::adlwrt(uint8_t reg, uint8_t value) {
    if (B(lbl::sndflg) == 0) return;   // the sound is off
    m_opl.write(reg, value);
}

void Sound::adlseq(uint8_t reg, int count, uint8_t value) {
    for (int i = 0; i < count; i++) adlwrt(static_cast<uint8_t>(reg + i), value);
}

void Sound::voiceon(uint8_t voice, uint16_t ax) {
    adlwrt(static_cast<uint8_t>(0xA0 + voice), static_cast<uint8_t>(ax & 0xFF));
    uint8_t al = static_cast<uint8_t>(((ax >> 8) << 2) | 1);   // the block, and a bit of fnum
    al |= 0x20;                                                 // key on
    adlwrt(static_cast<uint8_t>(0xB0 + voice), al);
}

void Sound::voiceoff(uint8_t voice, uint16_t ax) {
    uint8_t al = static_cast<uint8_t>(((ax >> 8) << 2) | 1);
    al &= 0xDF;   // key off
    adlwrt(static_cast<uint8_t>(0xB0 + voice), al);
}

// every register to a quiet default: the voices keyed off, percussion mode
void Sound::adlreginit() {
    constexpr int BASELEVEL = 9;
    (void)BASELEVEL;
    adlseq(0x40, 0x13, 63);   // all maximum level, ksl=0
    adlseq(0x53, 0x0D, 20);
    adlseq(0xB0, 8, 0x0D);    // keys off, octave to 3, fnum msbs
    adlwrt(1, 0);             // 0 to the test register
    adlwrt(1, 0x20);          // waveform select enable
    adlwrt(8, 0);             // 0 to csm and split
    adlseq(0x20, 0x20, 1);    // no am, vibrato, sustain, ksr; multiplier 1
    adlseq(0x60, 0x20, 0xF3); // attack 15, decay 3
    adlseq(0x80, 0x20, 0x03); // sustain level 0, release 3
    adlseq(0xA0, 8, 0);       // the tones of the voices to the middle
    adlwrt(0xBD, 0x20);       // percussion mode, the drums keyed off
    adlseq(0xC0, 8, 0);       // feedback 0, additive mode
    adlseq(0xE0, 0x16, 0);    // all the waveforms to sine
}

// writes pairs until a register of -1
void Sound::tblinit(uint32_t table) {
    for (uint32_t si = table;; si += 2) {
        const uint8_t reg = B(si);
        if (reg == 0xFF) return;
        adlwrt(reg, B(si + 1));
    }
}

void Sound::engineinit() { tblinit(lbl::enginetbl); }
void Sound::guninit() { tblinit(lbl::guntbl); }
void Sound::boominit() { tblinit(lbl::boomtbl); }
void Sound::blaminit() { tblinit(lbl::blamtbl); }
void Sound::screechinit() { tblinit(lbl::screechtbl); }
void Sound::saucerinit() { tblinit(lbl::saucertbl); }
void Sound::orbinit() { tblinit(lbl::orbtbl); }
void Sound::klainit() { tblinit(lbl::klatbl); }
void Sound::snareinit() { tblinit(lbl::snaretbl); }
void Sound::scrapeinit() { tblinit(lbl::scrapetbl); }
void Sound::windinit() { tblinit(lbl::windtbl); }
void Sound::stallinit() { tblinit(lbl::stalltbl); }

void Sound::snaresnd() {
    if (B(lbl::windflag) != 0) return;
    adlwrt(0xBD, 0x20);
    adlwrt(0xBD, 0x28);   // turn on the snare, leave the mode alone
}

void Sound::initvoices() {
    snareinit();
    guninit();
    boominit();
    // bassinit: voice 6 feedback, operator levels
    adlwrt(0xB6, 0x01);
    adlwrt(0x50, 0);
    adlwrt(0x53, 0);
    engineinit();
    screechinit();
    // (blaminit is left out: it shares the voice with the boom)
    stallinit();
    klainit();
    m_g.setW(lbl::enginefreq, 0x0100);   // a mid octave voice
}

// 3.ADL: twelve blocks of "reg,value" hex lines, each ended by "ff", for the tables in the order
// engine, gun, boom, screech, blam, snare, scrape, wind, stall, saucer, orb, kla. They are
// written over the start of the tables, whose old entries stay behind the new ones.
void Sound::readsounds(const std::string& adlFile) {
    std::FILE* f = std::fopen(adlFile.c_str(), "rb");
    if (!f) return;
    const uint32_t tables[12] = {lbl::enginetbl, lbl::guntbl, lbl::boomtbl, lbl::screechtbl,
                                 lbl::blamtbl,   lbl::snaretbl, lbl::scrapetbl, lbl::windtbl,
                                 lbl::stalltbl,  lbl::saucertbl, lbl::orbtbl,  lbl::klatbl};
    char line[128];
    for (uint32_t table : tables) {
        uint32_t di = table;
        for (int n = 0; n < 15; n++) {   // at most 14 register pairs
            if (!std::fgets(line, sizeof line, f)) {
                std::fclose(f);
                return;
            }
            char* end = nullptr;
            const long reg = std::strtol(line, &end, 16);
            if (end == line) {   // not a number: the original beeps and gives up
                std::fclose(f);
                return;
            }
            if (static_cast<uint8_t>(reg) == 0xFF) break;   // the end of this voice
            char* p = end;
            while (*p == ',' || *p == ' ') p++;
            const long val = std::strtol(p, nullptr, 16);
            SB(di++, static_cast<uint8_t>(reg));
            SB(di++, static_cast<uint8_t>(val));
        }
    }
    std::fclose(f);
}

void Sound::initsound(const std::string& adlFile) {
    adlreginit();
    readsounds(adlFile);
    initvoices();
}

// 3.ASM soundoff: all voices keyed off, the chip back to its quiet state
void Sound::soundoff() {
    for (uint8_t v = 0; v < 7; v++) voiceoff(v, 0x100);
    adlreginit();
}

// 3.ASM soundon
void Sound::soundon() {
    voiceoff(0, 0x100);
    initvoices();
    voiceoff(0, 0x100);
    if (B(lbl::ejectflag) != 0) windinit();
    SB(lbl::sauceronflag, 0);
    SB(lbl::orbonflag, 0);
    m_g.setW(lbl::oldkladist, 64);   // pretend no KLA in sight
}

void Sound::b3laminit() {   // the boom voice becomes a blam
    blaminit();
    m_g.setW(lbl::blamtick, -1);
}

void Sound::b3oominit() {   // and back
    boominit();
    m_g.setW(lbl::blamtick, 0);
}

} // namespace corncob
