// FrontEnd.hpp - The menus: what the original's MOAG.EXE showed between the missions
//
// MOAG.EXE has no source and is packed; its text (menus, prompts, reports) is in the file, and the rest is
// what CORNCOB.DOC and MOAG.INF describe. The screens here follow those: the main menu, the duty roster
// (pilots), combat missions (theaters of operation), training missions, the high score lists, the music
// and control choices, the documentation. What a mission is flown with is a MissionRequest, handed to the
// launcher the caller gives; the status the mission ends with comes back in a MissionOutcome.

#pragma once
#include "game/MissionRunner.hpp"
#include "ui/MusicBox.hpp"
#include "ui/Settings.hpp"
#include "ui/TextScreen.hpp"
#include <functional>
#include <memory>

namespace corncob {

class Career;   // the pilots, theaters and score lists (ui/Career.hpp)

using MissionLauncher = std::function<MissionOutcome(const MissionRequest&)>;

class FrontEnd {
public:
    FrontEnd(TextScreen& screen, Settings& settings, MusicBox* music, MissionLauncher launch);
    ~FrontEnd();

    // The main menu, until the player exits (or closes the window)
    void run();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace corncob
