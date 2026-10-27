// Opl.hpp - An AdLib (Yamaha YM3812 / OPL2) card: the chip emulation and the sound output
//
// The chip is ymfm's (third_party/ymfm, BSD). Register writes come from the game thread; SDL's
// audio thread pulls samples. Both take the audio lock, so a write lands between two samples.
// The device is asked for the chip's own rate (49716 Hz) and SDL converts if the sound card
// wants something else.

#pragma once
#include <SDL.h>
#include <cstdint>
#include <deque>
#include <memory>

namespace corncob {

class Opl {
public:
    Opl();
    ~Opl();
    Opl(const Opl&) = delete;
    Opl& operator=(const Opl&) = delete;

    // Opens the audio device; false if there is none (the chip still works for render())
    bool open();
    bool isOpen() const { return m_dev != 0; }

    // outp 388h / 389h: select a register, write it (what adlwrt does)
    void write(uint8_t reg, uint8_t value);

    // Mono 16 bit samples at sampleRate(), for the audio callback and for tests
    void render(int16_t* out, int frames);
    static constexpr int sampleRate() { return 49716; }   // 3579545 Hz / 72

    void setVolume(float v) { m_volume = v; }

private:
    struct Chip;
    static void audioCallback(void* user, uint8_t* stream, int len);

    std::unique_ptr<Chip> m_chip;
    SDL_AudioDeviceID m_dev = 0;
    float m_volume = 1.0f;
    std::deque<int16_t> m_pending;   // samples the chip made while a key write was being 'waited out' (see write)
    int16_t next();
};

} // namespace corncob
