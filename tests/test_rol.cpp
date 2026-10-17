// test_rol.cpp - The ROL/BNK music: the shipped songs parse, every instrument they name is in the bank,
// and playing them on the OPL chip makes sound at the right pitch
// Needs the original files (assets/original, never committed); skipped when missing.

#define SDL_MAIN_HANDLED
#include "audio/Opl.hpp"
#include "audio/RolPlayer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#ifndef CORNCOB_ASSET_DIR
#define CORNCOB_ASSET_DIR "assets/original"
#endif

using namespace corncob;

static int failures = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                      \
        }                                                                    \
    } while (0)

// the strongest frequency (Hz) in a block of samples, by counting zero crossings of a smoothed copy
static double dominantHz(const std::vector<int16_t>& s) {
    int crossings = 0;
    for (size_t i = 1; i < s.size(); i++)
        if ((s[i - 1] < 0) != (s[i] < 0)) crossings++;
    return crossings / 2.0 / (static_cast<double>(s.size()) / Opl::sampleRate());
}

int main() {
    const std::string dir = std::string(CORNCOB_ASSET_DIR) + "/";
    Bnk bank;
    if (!bank.load(dir + "CCMUSIC.BNK")) {
        std::printf("test_rol: original files not found, skipped\n");
        return 0;
    }
    CHECK(bank.size() > 100);
    CHECK(bank.find("ACCORDN") != nullptr);
    CHECK(bank.find("accordn") != nullptr);   // not case sensitive

    for (const char* name : {"DESTINY3.ROL", "DREAMSO2.ROL", "RIDINGT.ROL"}) {
        RolSong song;
        CHECK(song.load(dir + name));
        const uint32_t ticks = song.totalTicks();
        const double seconds = song.secondsFor(ticks);
        int missing = 0, notes = 0;
        std::set<std::string> names;
        for (const RolVoice& v : song.voices) {
            notes += static_cast<int>(v.notes.size());
            for (const RolTimbre& t : v.timbres)
                if (!bank.find(t.name)) {
                    missing++;
                    names.insert(t.name);
                }
        }
        std::printf("%s: mode=%d voices=%zu ticks/beat=%d tempo=%.1f ticks=%u (%.1f s) notes=%d missing=%d\n", name, song.mode,
                    song.voices.size(), song.ticksPerBeat, song.basicTempo, ticks, seconds, notes, missing);
        for (const std::string& n : names) std::printf("   no instrument '%s'\n", n.c_str());
        CHECK(!song.voices.empty());
        CHECK(missing == 0);
        CHECK(seconds > 30 && seconds < 900);

        // play the first seconds and listen to the chip
        Opl opl;
        RolPlayer player(opl, bank);
        player.start(song, false);
        double energy = 0;
        std::vector<int16_t> buf(Opl::sampleRate() / 50);   // 20 ms
        for (int frame = 0; frame < 50 * 6; frame++) {   // six seconds
            player.update(0.02);
            opl.render(buf.data(), static_cast<int>(buf.size()));
            for (int16_t v : buf) energy += std::abs(static_cast<double>(v));
        }
        std::printf("   energy over 6 s: %.0f\n", energy);
        CHECK(energy > 1e6);
        CHECK(player.playing());
    }

    // one note on its own: the pitch of middle C (48) is 261.6 Hz, and A in the next octave (57 + 12) 880 Hz
    {
        RolSong song;
        song.ticksPerBeat = 12;
        song.basicTempo = 120;
        song.mode = 1;
        RolVoice v;
        v.notes = {{48, 24}, {69, 24}};
        v.timbres = {{0, "ACCORDN"}};
        song.voices.push_back(v);
        Opl opl;
        RolPlayer player(opl, bank);
        player.start(song, false);
        std::vector<int16_t> first, second;
        std::vector<int16_t> buf(Opl::sampleRate() / 100);
        for (int k = 0; k < 100; k++) {   // one second: each note lasts 24 ticks = 1 second at 12 ticks/beat, 120 bpm
            player.update(0.01);
            opl.render(buf.data(), static_cast<int>(buf.size()));
            if (k >= 20 && k < 80) first.insert(first.end(), buf.begin(), buf.end());
        }
        for (int k = 0; k < 100; k++) {
            player.update(0.01);
            opl.render(buf.data(), static_cast<int>(buf.size()));
            if (k >= 20 && k < 80) second.insert(second.end(), buf.begin(), buf.end());
        }
        const double f1 = dominantHz(first), f2 = dominantHz(second);
        std::printf("pitches: %.1f Hz and %.1f Hz\n", f1, f2);
        CHECK(f2 / f1 > 3.0 && f2 / f1 < 4.5);   // 69 - 48 = 21 semitones: 3.36 times
    }

    if (failures == 0) std::printf("test_rol: all passed\n");
    return failures ? 1 : 0;
}
