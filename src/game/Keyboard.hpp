// Keyboard.hpp - The original's keyboard handling: new9 (int 9), getkbit, readkey, scalekey
//
// 3.EXE reads scan codes straight from the keyboard controller. For every key it keeps
//   kbdtbl     one bit per scan code: is the key down (getkbit)
//   keytable   8 bytes per scan code: [0] down flag, [2] tick when pressed, [4] ticks the key
//              was down since the last read, [6] tick of the last read (readkey)
//   scncode    the last scan code, with bit 7 set for a release; most commands fire on the
//              release (cmp scncode,32h+80h), scnflg says a new code arrived
// Controls are time-integrated: scalekey gives how much of the time since its last call a
// key was held, scaled to 0..cx, so a tap is a small deflection and a hold is full.
//
// Events carry how many timer ticks ago they happened, so a key pressed and released inside
// one frame still counts for the right fraction.

#pragma once
#include "game/GameState.hpp"
#include "game/TickClock.hpp"
#include <SDL.h>
#include <cstdint>

namespace corncob {

class DosKeyboard {
public:
    DosKeyboard(GameState& gs, TickClock& clock) : m_g(gs), m_clock(clock) {}

    // Maps an SDL scancode to the PC/XT scan code the game expects (0 = unmapped)
    static uint8_t dosScanCode(SDL_Scancode sc);

    // new9: one key transition, `ticksAgo` timer ticks before now
    void key(uint8_t dosCode, bool down, uint16_t ticksAgo = 0);

    // getkbit: nonzero if the key is down
    uint8_t getkbit(uint8_t code) const;

    struct Read { uint16_t down; int16_t elapsed; };
    // readkey: ticks the key was down, and ticks elapsed, since the last read
    Read readkey(uint8_t code);
    // scalekey: fraction of the time since the last read that the key was down, 0..cx
    uint16_t scalekey(uint8_t code, uint16_t cx);

    // flush_keys / cleargetkey
    void clear();

    uint8_t scncode() const { return m_g.b(SCNCODE); }
    void setScncode(uint8_t v) { m_g.setB(SCNCODE, v); }

private:
    static const uint32_t SCNCODE;
    static const uint32_t SCNFLG;
    static const uint32_t KEYTABLE;
    static const uint32_t KBDTBL;

    GameState& m_g;
    TickClock& m_clock;
};

} // namespace corncob
