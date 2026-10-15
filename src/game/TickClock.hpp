// TickClock.hpp - Emulates the reprogrammed PIT timer from 3.ASM / 3MACS.INC (new08)
//
// The game sets PIT channel 0 to reload tickdiv = 16384 / (tickfactor/4) = 1024,
// i.e. 1193182 / 1024 = ~1165.2 interrupts per second. Each interrupt bumps the
// code-segment counters frmticks, oticks, dticks and tdticks. Every tickfactor
// (64) interrupts it chains to the BIOS handler (18.2 Hz) and increments `ticks`
// and counts down bultick/mistick (those two live in DGROUP and are handled by
// the caller via onSlowTick).

#pragma once
#include <cstdint>

namespace corncob {

class TickClock {
public:
    static constexpr int TICKFACTOR = 64;                 // 3.ASM tickfactor
    static constexpr int TICKDIV = 16384 / (TICKFACTOR / 4); // 3.ASM tickdiv = 1024
    static constexpr double PIT_HZ = 1193182.0;
    static constexpr double FAST_HZ = PIT_HZ / TICKDIV;   // ~1165.2 Hz

    // cs: variables from 3.ASM
    uint16_t ticks = 0;     // 18.2 Hz
    uint32_t dticks = 0;    // dw ?,? (32-bit)
    uint32_t tdticks = 0;   // dw ?,? (32-bit)
    uint16_t frmticks = 0;  // reset by the main loop each frame
    uint16_t oticks = 0;

    // Advance by real elapsed seconds; returns the number of slow (18.2 Hz) ticks
    // that occurred, so the caller can decrement bultick/mistick.
    int advance(double seconds) {
        m_accum += seconds * FAST_HZ;
        int slow = 0;
        while (m_accum >= 1.0) {
            m_accum -= 1.0;
            if (interrupt()) slow++;
        }
        return slow;
    }

    // One timer interrupt (new08). Returns true when it was also a BIOS tick.
    bool interrupt() {
        frmticks++;
        dticks++;
        tdticks++;
        oticks++;
        if (oticks & (TICKFACTOR - 1)) return false; // "just us"
        ticks++;
        return true;
    }

private:
    double m_accum = 0.0;
};

} // namespace corncob
