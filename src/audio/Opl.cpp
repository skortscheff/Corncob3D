// Opl.cpp - ymfm's YM3812 behind an SDL audio device

#include "Opl.hpp"
#include "ymfm_opl.h"
#include <algorithm>

namespace corncob {

struct Opl::Chip {
    ymfm::ymfm_interface iface;   // the default interface: no timers or IRQs are needed
    ymfm::ym3812 chip{iface};
};

Opl::Opl() : m_chip(new Chip) { m_chip->chip.reset(); }

Opl::~Opl() {
    if (m_dev) SDL_CloseAudioDevice(m_dev);
}

bool Opl::open() {
    if (m_dev) return true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) return false;
    SDL_AudioSpec want{}, have{};
    want.freq = sampleRate();
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = &Opl::audioCallback;
    want.userdata = this;
    m_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!m_dev) return false;
    SDL_PauseAudioDevice(m_dev, 0);
    return true;
}

// One sample from the chip
int16_t Opl::next() {
    ymfm::ym3812::output_data o;
    m_chip->chip.generate(&o, 1);
    const int v = static_cast<int>(o.data[0] * m_volume);
    return static_cast<int16_t>(std::clamp(v, -32768, 32767));
}

void Opl::write(uint8_t reg, uint8_t value) {
    if (m_dev) SDL_LockAudioDevice(m_dev);
    m_chip->chip.write(0, reg);
    m_chip->chip.write(1, value);
    // The card needed about 35 microseconds (a sample or two) after every write, and the game relies on it: a voice is keyed
    // off and on again straight away to restart a sound, which only works if the chip sees the key off for a moment. Here the
    // writes would land between two samples, the chip would never see it, and every shot after the first would be silent.
    // So after the key-on registers of the voices and the drums the chip runs two samples, played ahead of the rest.
    if ((reg >= 0xB0 && reg <= 0xB8) || reg == 0xBD) {
        m_pending.push_back(next());
        m_pending.push_back(next());
    }
    if (m_dev) SDL_UnlockAudioDevice(m_dev);
}

void Opl::render(int16_t* out, int frames) {
    for (int i = 0; i < frames; i++) {
        if (!m_pending.empty()) {
            out[i] = m_pending.front();
            m_pending.pop_front();
        } else {
            out[i] = next();
        }
    }
}

void Opl::audioCallback(void* user, uint8_t* stream, int len) {
    static_cast<Opl*>(user)->render(reinterpret_cast<int16_t*>(stream), len / 2);   // the device lock is held
}

} // namespace corncob
