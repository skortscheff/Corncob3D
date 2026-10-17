// RolPlayer.hpp - AdLib Visual Composer music: .ROL songs with their .BNK instrument banks
//
// The title and menu music of the original (CCMUSIC.BNK, DESTINY3.ROL, DREAMSO2.ROL, RIDINGT.ROL) is
// played by the sourceless CCTITLE/MOAG programs through the AdLib card; this player does the same on
// the OPL chip of the port (audio/Opl). The formats, as decoded from the shipped files:
//
//   BNK  28 byte header (u8 major, u8 minor, "ADLIB-", u16 used, u16 total, u32 name offset, u32 data
//        offset), name records of 12 bytes (u16 index, u8 used, char name[9]) at the name offset, and
//        30 byte instruments at the data offset (u8 percussive, u8 voice, two operators of 13 bytes -
//        ksl, multiple, feedback, attack, sustain level, eg type, decay, release, total level, am, vib,
//        ksr, connection - and two wave selects).
//   ROL  header of 182 bytes (u16 version, 40 byte name, u16 ticks per beat, beats per measure, two
//        editing scales, a byte, the mode (0 percussive: 11 voices, else melodic: 9), filler), then the
//        tempo track (char name[15], f32 basic tempo, u16 count, count x {u16 time, f32 multiplier}),
//        then per voice: the notes (name[15], u16 total ticks, {u16 note, u16 duration} until the
//        durations add up; note 0 is a rest, 48 is middle C), the timbres (name[15], u16 count,
//        count x {u16 time, char instrument[9], u8, u16}), the volumes and the pitches (name[15],
//        u16 count, count x {u16 time, f32 value}).

#pragma once
#include "audio/Opl.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace corncob {

struct BnkOperator {
    uint8_t ksl = 0, multiple = 0, feedback = 0, attack = 0, sustain = 0, egType = 0, decay = 0, release = 0;
    uint8_t totalLevel = 0, am = 0, vib = 0, ksr = 0, connection = 0;
};

struct BnkInstrument {
    uint8_t percussive = 0, voice = 0;
    BnkOperator op[2];
    uint8_t wave[2] = {0, 0};
};

class Bnk {
public:
    bool load(const std::string& path);
    // Case-insensitive; null when the bank has no such instrument
    const BnkInstrument* find(const std::string& name) const;
    size_t size() const { return m_names.size(); }

private:
    std::vector<std::string> m_names;   // upper case
    std::vector<BnkInstrument> m_ins;
};

struct RolNote { uint16_t note, duration; };
struct RolTimbre { uint16_t time; std::string name; };
struct RolValue { uint16_t time; float value; };

struct RolVoice {
    std::vector<RolNote> notes;
    std::vector<RolTimbre> timbres;
    std::vector<RolValue> volumes;
    std::vector<RolValue> pitches;
};

struct RolSong {
    uint16_t ticksPerBeat = 0;
    uint8_t mode = 0;           // 0 percussive (11 voices), else melodic (9)
    float basicTempo = 120;
    std::vector<RolValue> tempo;
    std::vector<RolVoice> voices;

    bool load(const std::string& path);
    uint32_t totalTicks() const;   // the end of the longest voice
    // seconds a given number of ticks take, following the tempo track
    double secondsFor(uint32_t ticks) const;
};

class RolPlayer {
public:
    RolPlayer(Opl& opl, const Bnk& bank) : m_opl(opl), m_bank(bank) {}

    // Takes the song (it must outlive the player) and starts it from the beginning
    void start(const RolSong& song, bool loop);
    void stop();
    // Moves the song on by dt seconds, writing the chip registers for what happens
    void update(double dt);
    bool playing() const { return m_song != nullptr && !m_done; }

private:
    struct VoiceState {
        size_t note = 0, timbre = 0, volume = 0, pitch = 0;
        double noteEnd = 0;       // the tick the current note ends on
        bool keyOn = false;
        int curNote = 0;
        float vol = 1.0f, bend = 1.0f;
        const BnkInstrument* ins = nullptr;
        bool started = false;
    };

    void reset();
    void loadInstrument(int voice, const BnkInstrument& ins);
    void setVolume(int voice);
    void keyOn(int voice, int note);
    void keyOff(int voice);
    void writeFreq(int voice, int note, bool on);
    double ticksPerSecond() const;

    Opl& m_opl;
    const Bnk& m_bank;
    const RolSong* m_song = nullptr;
    std::vector<VoiceState> m_vs;
    double m_tick = 0;
    size_t m_tempoIndex = 0;
    double m_mult = 1.0;
    bool m_loop = false, m_done = true;
    uint8_t m_bd = 0;   // the rhythm register (0xBD) as written
};

} // namespace corncob
