// Sound.hpp - ADL.ASM: the AdLib sound effects
//
// Each effect is a table of (register, value) pairs, ended by 0FFh, that tblinit writes to the
// chip: a patch for one of the nine voices (engine 0, guns 1, booms and blams 2, screech /
// saucer / orb 3, KLA 4, stall alarm 5) or the snare of the percussion section. The game keys
// voices on and off with voiceon / voiceoff and shapes them by writing single registers
// (adlwrt). The tables live in the DGROUP image, and 3.ADL can replace them.
//
// adlwrt does nothing while the sound is off (sndflg, the S key).

#pragma once
#include "audio/Opl.hpp"
#include "game/GameState.hpp"
#include <cstdint>
#include <string>

namespace corncob {

class Sound {
public:
    Sound(GameState& gs, Opl& opl) : m_g(gs), m_opl(opl) {}

    void adlwrt(uint8_t reg, uint8_t value);
    void adlseq(uint8_t reg, int count, uint8_t value);
    // ax = fnum bits (low byte: fnum, high byte: the block), bl = voice 0-8
    void voiceon(uint8_t voice, uint16_t ax);
    void voiceoff(uint8_t voice, uint16_t ax);
    void adlreginit();

    // the effects
    void tblinit(uint32_t table);
    void engineinit();
    void guninit();
    void boominit();
    void blaminit();
    void screechinit();
    void saucerinit();
    void orbinit();
    void klainit();
    void snareinit();
    void scrapeinit();
    void windinit();
    void stallinit();
    void snaresnd();   // keys the snare on (not while the wind blows)

    // initsound: clears the chip, reads 3.ADL, sets up all the voices
    void initsound(const std::string& adlFile);
    void initvoices();
    void readsounds(const std::string& adlFile);

    // 3.ASM: all voices off / on again; the boom voice changed into a blam and back
    void soundoff();
    void soundon();
    void b3laminit();
    void b3oominit();

private:
    uint8_t B(uint32_t o) const { return m_g.b(o); }
    void SB(uint32_t o, uint8_t v) { m_g.setB(o, v); }

    GameState& m_g;
    Opl& m_opl;
};

} // namespace corncob
