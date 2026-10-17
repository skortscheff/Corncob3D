// MusicBox.cpp - The title and menu music (see MusicBox.hpp)

#include "MusicBox.hpp"
#include "assets/Assets.hpp"
#include <SDL.h>
#include <chrono>
#include <cstdio>
#include <fstream>

namespace corncob {

MusicBox::MusicBox() = default;

MusicBox::~MusicBox() {
    m_stopThread = true;
    if (m_thread.joinable()) m_thread.join();
}

bool MusicBox::init() {
    if (m_ok) return true;
    if (!m_bank.load(assetPath("CCMUSIC.BNK"))) return false;
    // the song list of ROLFILES.TXT, one file name a line
    std::ifstream list(assetPath("ROLFILES.TXT"));
    std::string line;
    while (std::getline(list, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;
        RolSong song;
        if (song.load(assetPath(line))) m_songs.push_back(std::move(song));
    }
    m_title.load(assetPath("DESTINY3.ROL"));
    if (m_songs.empty() && m_title.voices.empty()) return false;
    if (!reopen()) return false;
    m_ok = true;
    m_stopThread = false;
    m_thread = std::thread([this] { run(); });
    return true;
}

bool MusicBox::reopen() {
    std::lock_guard<std::mutex> g(m_lock);
    if (m_opl) return true;
    m_opl = std::make_unique<Opl>();
    if (!m_opl->open()) {
        m_opl.reset();
        return false;
    }
    m_player = std::make_unique<RolPlayer>(*m_opl, m_bank);
    return true;
}

void MusicBox::close() {
    std::lock_guard<std::mutex> g(m_lock);
    if (m_player) m_player->stop();
    m_playing = false;
    m_player.reset();
    m_opl.reset();   // the sound card goes back to the game
}

void MusicBox::startSong(const RolSong& song, bool loop) {
    std::lock_guard<std::mutex> g(m_lock);
    if (!m_player || song.voices.empty()) return;
    for (int c = 0; c < 9; c++) m_opl->write(static_cast<uint8_t>(0xB0 + c), 0);   // silence what was playing
    m_player->start(song, loop);
    m_playing = true;
}

void MusicBox::playTitle() {
    if (!m_ok || !m_on) return;
    startSong(m_title.voices.empty() && !m_songs.empty() ? m_songs[0] : m_title, false);
}

void MusicBox::playMenu() {
    if (!m_ok || !m_on || m_songs.empty()) return;
    const RolSong& s = m_songs[m_next % m_songs.size()];
    startSong(s, true);
}

void MusicBox::stop() {
    std::lock_guard<std::mutex> g(m_lock);
    if (m_player) m_player->stop();
    m_playing = false;
}

void MusicBox::setOn(bool on) {
    m_on = on;
    if (!m_ok) return;
    if (!on) {
        stop();
    } else {
        m_next++;   // the next song
        playMenu();
    }
}

void MusicBox::run() {
    using clock = std::chrono::steady_clock;
    auto prev = clock::now();
    while (!m_stopThread) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        const auto now = clock::now();
        const double dt = std::chrono::duration<double>(now - prev).count();
        prev = now;
        std::lock_guard<std::mutex> g(m_lock);
        if (m_player && m_player->playing()) m_player->update(std::min(dt, 0.1));
        else m_playing = false;
    }
}

} // namespace corncob
