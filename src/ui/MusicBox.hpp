// MusicBox.hpp - The title and menu music (the original's "music system" of CCTITLE and MOAG)
//
// Plays the songs listed in ROLFILES.TXT (RIDINGT, DREAMSO2) one after the other on its own OPL chip,
// from a thread of its own, so the title sequence and every menu screen can run without feeding it.
// DESTINY3.ROL is not on that list: it is the title song. "The music only plays during the title
// credits and menus. If you want to switch to the next song, just turn the sound off and then on
// again" (MOAG.INF): toggle() does that.

#pragma once
#include "audio/Opl.hpp"
#include "audio/RolPlayer.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace corncob {

class MusicBox {
public:
    MusicBox();
    ~MusicBox();

    // Loads the bank and songs from the game directory and opens the sound; false if there is nothing to
    // play (the box then does nothing)
    bool init();
    bool available() const { return m_ok; }

    void playTitle();     // the title song, once
    void playMenu();      // the next menu song, looping
    void stop();          // silence
    bool playing() const { return m_playing; }
    // The music toggle of the main menu: off, or on with the next song
    void setOn(bool on);
    bool on() const { return m_on; }
    // For a mission: gives the sound card back (the game opens its own)
    void close();
    bool reopen();

private:
    void run();
    void startSong(const RolSong& song, bool loop);

    Bnk m_bank;
    RolSong m_title;
    std::vector<RolSong> m_songs;
    std::unique_ptr<Opl> m_opl;
    std::unique_ptr<RolPlayer> m_player;
    std::mutex m_lock;
    std::thread m_thread;
    std::atomic<bool> m_stopThread{false};
    bool m_ok = false;
    std::atomic<bool> m_playing{false};
    bool m_on = true;
    size_t m_next = 0;
};

} // namespace corncob
