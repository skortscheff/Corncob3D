// RolPlayer.cpp - AdLib Visual Composer music (see RolPlayer.hpp for the file formats)

#include "RolPlayer.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace corncob {

namespace {

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? static_cast<size_t>(n) : 0);
    const size_t got = out.empty() ? 0 : std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return got == out.size();
}

struct Reader {
    const std::vector<uint8_t>& d;
    size_t pos = 0;
    bool ok = true;
    bool need(size_t n) {
        if (pos + n > d.size()) ok = false;
        return ok;
    }
    uint8_t u8() { return need(1) ? d[pos++] : 0; }
    uint16_t u16() {
        if (!need(2)) return 0;
        const uint16_t v = static_cast<uint16_t>(d[pos] | (d[pos + 1] << 8));
        pos += 2;
        return v;
    }
    uint32_t u32() {
        const uint32_t lo = u16();
        return lo | (static_cast<uint32_t>(u16()) << 16);
    }
    float f32() {
        const uint32_t v = u32();
        float f;
        std::memcpy(&f, &v, 4);
        return f;
    }
    void skip(size_t n) {
        if (need(n)) pos += n;
    }
    std::string str(size_t n) {   // a fixed field holding a zero terminated string
        if (!need(n)) return {};
        std::string s;
        for (size_t i = 0; i < n && d[pos + i]; i++) s += static_cast<char>(d[pos + i]);
        pos += n;
        return s;
    }
};

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

// ==========================================================================================
// the bank
// ==========================================================================================
bool Bnk::load(const std::string& path) {
    std::vector<uint8_t> data;
    if (!readFile(path, data)) return false;
    Reader r{data};
    r.skip(2);   // version
    if (r.str(6) != "ADLIB-") return false;
    const uint16_t used = r.u16();
    r.u16();   // total
    const uint32_t nameOff = r.u32(), dataOff = r.u32();
    if (!r.ok) return false;
    m_names.clear();
    m_ins.clear();
    for (uint16_t i = 0; i < used; i++) {
        Reader n{data, nameOff + 12u * i};
        const uint16_t index = n.u16();
        n.u8();   // flags
        const std::string name = n.str(9);
        Reader ir{data, dataOff + 30u * index};
        BnkInstrument ins;
        ins.percussive = ir.u8();
        ins.voice = ir.u8();
        for (int o = 0; o < 2; o++) {
            BnkOperator& op = ins.op[o];
            op.ksl = ir.u8();
            op.multiple = ir.u8();
            op.feedback = ir.u8();
            op.attack = ir.u8();
            op.sustain = ir.u8();
            op.egType = ir.u8();
            op.decay = ir.u8();
            op.release = ir.u8();
            op.totalLevel = ir.u8();
            op.am = ir.u8();
            op.vib = ir.u8();
            op.ksr = ir.u8();
            op.connection = ir.u8();
        }
        ins.wave[0] = ir.u8();
        ins.wave[1] = ir.u8();
        if (!n.ok || !ir.ok) continue;
        m_names.push_back(upper(name));
        m_ins.push_back(ins);
    }
    return !m_ins.empty();
}

const BnkInstrument* Bnk::find(const std::string& name) const {
    const std::string key = upper(name);
    for (size_t i = 0; i < m_names.size(); i++)
        if (m_names[i] == key) return &m_ins[i];
    return nullptr;
}

// ==========================================================================================
// the song
// ==========================================================================================
bool RolSong::load(const std::string& path) {
    std::vector<uint8_t> data;
    if (!readFile(path, data)) return false;
    Reader r{data};
    r.skip(2 + 2 + 40);   // version, name
    ticksPerBeat = r.u16();
    r.skip(2 + 2 + 2 + 1);   // beats per measure, the two editing scales, a byte
    mode = r.u8();
    r.pos = 182;   // the filler is 128 bytes
    // the tempo track
    r.skip(15);
    basicTempo = r.f32();
    const uint16_t nTempo = r.u16();
    tempo.clear();
    for (uint16_t i = 0; i < nTempo && r.ok; i++) {
        RolValue v;
        v.time = r.u16();
        v.value = r.f32();
        tempo.push_back(v);
    }
    if (!r.ok) return false;
    // the voices
    voices.clear();
    while (r.ok && r.pos + 17 <= data.size()) {
        RolVoice v;
        r.skip(15);
        const uint16_t total = r.u16();
        uint32_t sum = 0;
        while (sum < total && r.ok) {
            RolNote n;
            n.note = r.u16();
            n.duration = r.u16();
            if (n.duration == 0) {
                r.ok = false;
                break;
            }
            sum += n.duration;
            v.notes.push_back(n);
        }
        r.skip(15);
        const uint16_t nTimbre = r.u16();
        for (uint16_t i = 0; i < nTimbre && r.ok; i++) {
            RolTimbre t;
            t.time = r.u16();
            t.name = r.str(9);
            r.skip(1 + 2);
            v.timbres.push_back(t);
        }
        r.skip(15);
        const uint16_t nVolume = r.u16();
        for (uint16_t i = 0; i < nVolume && r.ok; i++) {
            RolValue e;
            e.time = r.u16();
            e.value = r.f32();
            v.volumes.push_back(e);
        }
        r.skip(15);
        const uint16_t nPitch = r.u16();
        for (uint16_t i = 0; i < nPitch && r.ok; i++) {
            RolValue e;
            e.time = r.u16();
            e.value = r.f32();
            v.pitches.push_back(e);
        }
        if (!r.ok) break;
        voices.push_back(std::move(v));
    }
    return !voices.empty();
}

uint32_t RolSong::totalTicks() const {
    uint32_t best = 0;
    for (const RolVoice& v : voices) {
        uint32_t sum = 0;
        for (const RolNote& n : v.notes) sum += n.duration;
        best = std::max(best, sum);
    }
    return best;
}

double RolSong::secondsFor(uint32_t ticks) const {
    double t = 0, at = 0, mult = 1.0;
    size_t i = 0;
    while (at < ticks) {
        double next = ticks;
        while (i < tempo.size() && tempo[i].time <= at) mult = tempo[i++].value;
        if (i < tempo.size()) next = std::min<double>(next, tempo[i].time);
        const double tps = ticksPerBeat * basicTempo * mult / 60.0;
        t += (next - at) / (tps > 0 ? tps : 1);
        if (next <= at) break;
        at = next;
    }
    return t;
}

// ==========================================================================================
// the player
// ==========================================================================================
namespace {

constexpr int MODOFF[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};

// the operators of a percussive voice (6 = bass drum, 7 snare, 8 tom, 9 cymbal, 10 hi-hat)
struct Drum { int op1, op2, channel, bit; };
constexpr Drum DRUMS[5] = {{0x10, 0x13, 6, 0x10}, {0x14, -1, 7, 0x08}, {0x12, -1, 8, 0x04}, {0x15, -1, 8, 0x02}, {0x11, -1, 7, 0x01}};

} // namespace

double RolPlayer::ticksPerSecond() const {
    return m_song ? m_song->ticksPerBeat * m_song->basicTempo * m_mult / 60.0 : 1.0;
}

void RolPlayer::reset() {
    m_opl.write(0x01, 0x20);   // enable the wave select bits
    m_bd = m_song && m_song->mode == 0 ? 0x20 : 0x00;   // percussive songs use the rhythm section
    m_opl.write(0xBD, m_bd);
    for (int c = 0; c < 9; c++) m_opl.write(static_cast<uint8_t>(0xB0 + c), 0);
    m_vs.assign(m_song ? m_song->voices.size() : 0, VoiceState{});
    m_tick = 0;
    m_tempoIndex = 0;
    m_mult = 1.0;
    m_done = false;
}

void RolPlayer::start(const RolSong& song, bool loop) {
    m_song = &song;
    m_loop = loop;
    reset();
}

void RolPlayer::stop() {
    if (!m_song) return;
    for (size_t v = 0; v < m_vs.size(); v++)
        if (m_vs[v].keyOn) keyOff(static_cast<int>(v));
    m_done = true;
}

// writes an instrument into the operators of a voice
void RolPlayer::loadInstrument(int voice, const BnkInstrument& ins) {
    auto writeOp = [&](int off, const BnkOperator& op, uint8_t wave, int tl) {
        m_opl.write(static_cast<uint8_t>(0x20 + off), static_cast<uint8_t>((op.am << 7) | (op.vib << 6) | (op.egType << 5) | (op.ksr << 4) | (op.multiple & 15)));
        m_opl.write(static_cast<uint8_t>(0x40 + off), static_cast<uint8_t>((op.ksl << 6) | (tl & 63)));
        m_opl.write(static_cast<uint8_t>(0x60 + off), static_cast<uint8_t>((op.attack << 4) | (op.decay & 15)));
        m_opl.write(static_cast<uint8_t>(0x80 + off), static_cast<uint8_t>((op.sustain << 4) | (op.release & 15)));
        m_opl.write(static_cast<uint8_t>(0xE0 + off), static_cast<uint8_t>(wave & 3));
    };
    const bool drums = m_song->mode == 0 && voice >= 6;
    if (!drums) {
        const int m = MODOFF[voice];
        writeOp(m, ins.op[0], ins.wave[0], ins.op[0].totalLevel);
        writeOp(m + 3, ins.op[1], ins.wave[1], ins.op[1].totalLevel);
        m_opl.write(static_cast<uint8_t>(0xC0 + voice), static_cast<uint8_t>(((ins.op[0].feedback & 7) << 1) | (ins.op[0].connection & 1)));
    } else {
        const Drum& dr = DRUMS[voice - 6];
        writeOp(dr.op1, ins.op[0], ins.wave[0], ins.op[0].totalLevel);
        if (dr.op2 >= 0) {   // the bass drum has two operators
            writeOp(dr.op2, ins.op[1], ins.wave[1], ins.op[1].totalLevel);
            m_opl.write(static_cast<uint8_t>(0xC0 + dr.channel), static_cast<uint8_t>(((ins.op[0].feedback & 7) << 1) | (ins.op[0].connection & 1)));
        }
    }
}

// the volume of a voice: the carrier's total level moves towards silence as the volume falls
void RolPlayer::setVolume(int voice) {
    const VoiceState& s = m_vs[voice];
    if (!s.ins) return;
    const bool drums = m_song->mode == 0 && voice >= 6;
    auto level = [&](const BnkOperator& op) {
        const float v = std::max(0.0f, std::min(1.0f, s.vol));
        return static_cast<int>(63 - (63 - op.totalLevel) * v + 0.5f);
    };
    if (!drums) {
        const int m = MODOFF[voice];
        m_opl.write(static_cast<uint8_t>(0x40 + m + 3), static_cast<uint8_t>((s.ins->op[1].ksl << 6) | level(s.ins->op[1])));
        if (s.ins->op[0].connection & 1)   // additive: the modulator is heard as well
            m_opl.write(static_cast<uint8_t>(0x40 + m), static_cast<uint8_t>((s.ins->op[0].ksl << 6) | level(s.ins->op[0])));
    } else {
        const Drum& dr = DRUMS[voice - 6];
        const int op = dr.op2 >= 0 ? dr.op2 : dr.op1;
        const BnkOperator& o = dr.op2 >= 0 ? s.ins->op[1] : s.ins->op[0];
        m_opl.write(static_cast<uint8_t>(0x40 + op), static_cast<uint8_t>((o.ksl << 6) | level(o)));
    }
}

// the F number and block of a note (48 is middle C) and the bend, written to the voice's channel
void RolPlayer::writeFreq(int voice, int note, bool on) {
    const bool drums = m_song->mode == 0 && voice >= 6;
    const int ch = drums ? DRUMS[voice - 6].channel : voice;
    const double hz = 261.6256 * std::pow(2.0, (note - 48) / 12.0) * m_vs[voice].bend;
    int block = std::max(0, std::min(7, note / 12));
    double fnum = hz * std::pow(2.0, 20 - block) / Opl::sampleRate();
    while (fnum > 1023 && block < 7) {
        block++;
        fnum /= 2;
    }
    while (fnum < 256 && block > 0) {   // keep the best resolution
        block--;
        fnum *= 2;
    }
    const int f = std::max(0, std::min(1023, static_cast<int>(fnum + 0.5)));
    m_opl.write(static_cast<uint8_t>(0xA0 + ch), static_cast<uint8_t>(f & 0xFF));
    const bool melodicKey = on && !drums;
    m_opl.write(static_cast<uint8_t>(0xB0 + ch), static_cast<uint8_t>((melodicKey ? 0x20 : 0) | (block << 2) | (f >> 8)));
}

void RolPlayer::keyOn(int voice, int note) {
    VoiceState& s = m_vs[voice];
    s.curNote = note;
    writeFreq(voice, note, true);
    if (m_song->mode == 0 && voice >= 6) {
        m_bd = static_cast<uint8_t>(m_bd | DRUMS[voice - 6].bit);
        m_opl.write(0xBD, m_bd);
    }
    s.keyOn = true;
}

void RolPlayer::keyOff(int voice) {
    VoiceState& s = m_vs[voice];
    if (m_song->mode == 0 && voice >= 6) {
        m_bd = static_cast<uint8_t>(m_bd & ~DRUMS[voice - 6].bit);
        m_opl.write(0xBD, m_bd);
    } else {
        writeFreq(voice, s.curNote, false);
    }
    s.keyOn = false;
}

void RolPlayer::update(double dt) {
    if (!m_song || m_done) return;
    const uint32_t total = m_song->totalTicks();
    while (dt > 0 && !m_done) {
        const double step = std::min(dt, 0.005);
        dt -= step;
        m_tick += step * ticksPerSecond();
        while (m_tempoIndex < m_song->tempo.size() && m_song->tempo[m_tempoIndex].time <= m_tick)
            m_mult = m_song->tempo[m_tempoIndex++].value;

        for (size_t vi = 0; vi < m_vs.size(); vi++) {
            const int voice = static_cast<int>(vi);
            if (m_song->mode != 0 && voice >= 9) break;   // a melodic song has nine voices
            if (m_song->mode == 0 && voice >= 11) break;
            const RolVoice& rv = m_song->voices[vi];
            VoiceState& s = m_vs[vi];
            while (s.timbre < rv.timbres.size() && rv.timbres[s.timbre].time <= m_tick) {
                if (const BnkInstrument* ins = m_bank.find(rv.timbres[s.timbre].name)) {
                    s.ins = ins;
                    loadInstrument(voice, *ins);
                    setVolume(voice);
                }
                s.timbre++;
            }
            bool volChanged = false;
            while (s.volume < rv.volumes.size() && rv.volumes[s.volume].time <= m_tick) {
                s.vol = rv.volumes[s.volume++].value;
                volChanged = true;
            }
            if (volChanged) setVolume(voice);
            bool bent = false;
            while (s.pitch < rv.pitches.size() && rv.pitches[s.pitch].time <= m_tick) {
                s.bend = rv.pitches[s.pitch++].value;
                bent = true;
            }
            if (bent && s.keyOn) writeFreq(voice, s.curNote, true);
            // the notes: one ends where the next begins
            while (s.note <= rv.notes.size() && m_tick >= s.noteEnd) {
                if (s.keyOn) keyOff(voice);
                if (s.note >= rv.notes.size()) break;
                const RolNote& n = rv.notes[s.note++];
                s.noteEnd += n.duration;
                if (n.note != 0 && s.ins) keyOn(voice, n.note);
            }
        }
        if (m_tick >= total) {
            for (size_t v = 0; v < m_vs.size(); v++)
                if (m_vs[v].keyOn) keyOff(static_cast<int>(v));
            if (m_loop) {
                const RolSong* song = m_song;
                m_song = song;
                reset();
            } else {
                m_done = true;
            }
        }
    }
}

} // namespace corncob
