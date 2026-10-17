// Career.hpp - The pilots, their theaters of operation and the score lists (what MOAG kept on disk)
//
// MOAG.EXE kept this in pilot.hsc (the high score lists), a theater file per pilot (tht\N.tht) and the
// universe copies those theaters need (the history: what a pilot has destroyed stays destroyed). Here it
// is one text file (career.dat) and one binary universe file per open theater (theater<N>.cct) in the
// save directory. The rules are CORNCOB.DOC's and MOAG.INF's:
//   * a pilot can have up to 15 theaters open, one of them the current one; closing one leaves the record
//   * a pilot killed in action or captured is deactivated and can only be resurrected (the number of
//     resurrections is kept with the name); a captured pilot is confined to the stockade for a while
//   * deleting a pilot freezes the career in the lists, where the name is marked with a *
//   * three lists of ten: high scores (single missions), top careers (total points) and fastest careers
//     (points per mission, at least three missions)

#pragma once
#include "ui/Scoring.hpp"
#include <string>
#include <vector>

namespace corncob {

constexpr int MAX_PILOTS = 20;
constexpr int MAX_THEATERS = 15;
constexpr size_t MAX_NAME = 18;

struct Stats {
    int missions = 0;
    long points = 0;
    long best = 0;               // the best single mission
    Tally kills, bestKills;      // all the kills, and those of the best mission
    int planesLost = 0;
    int friendliesLost = 0;
    long damage = 0;
    long flightSeconds = 0;
    int survived = 0;            // missions not ended by death or capture
    int missionsWithoutLoss = 0;
};

struct TheaterRecord {
    int id = 0;                  // the number of its universe file
    std::string def;             // the theater: MORNING, DUSK ...
    Stats stats;
    int objectivesTotal = 0, objectivesRemaining = 0;
    bool complete = false;
    int startTower = 0;          // the airfield the next mission starts from
    int planesLeft = 0;          // regular planes in the theater
    int storagePlanes = 3;       // older planes that can be brought out of storage
};

enum class PilotStatus { Ready, Killed, Captured, Stockade };

struct Pilot {
    int id = 0;                  // never reused: the lists refer to it
    std::string name;
    PilotStatus status = PilotStatus::Ready;
    std::string releaseDate;     // while confined to the stockade
    int resurrections = 0;
    Stats career;
    unsigned medals = 0;         // a bit for each Medal
    int rank = 0;
    int bestScoreTheater = 0;
    std::vector<TheaterRecord> theaters;
    std::vector<std::string> completed;   // the theaters (def names) this pilot has completed
    int current = -1;            // index of the current theater
    bool dead() const { return status != PilotStatus::Ready; }
};

struct ScoreEntry {
    int pilotId = 0;
    std::string name;
    bool deleted = false;
    long score = 0;              // the mission, or the career
    std::string theater;
    int missions = 0, resurrections = 0;
    int objectives = 0, primary = 0, other = 0;
    long average = 0;
};

enum class ScoreList { HighScores, TopCareers, FastCareers };

class Career {
public:
    // The files live in `dir` (with a trailing separator); a missing file means an empty career
    explicit Career(const std::string& dir);
    bool load();
    bool save() const;

    std::vector<Pilot>& pilots() { return m_pilots; }
    const std::vector<Pilot>& pilots() const { return m_pilots; }
    int activeIndex() const { return m_active; }
    Pilot* active() { return m_active >= 0 ? &m_pilots[m_active] : nullptr; }
    int findPilot(const std::string& name) const;   // case insensitive, -1 if unknown

    // pilots
    int createPilot(const std::string& name);       // -1 when the list is full or the name is taken
    bool activate(int index);                       // false for a pilot who can't fly
    void deactivate() { m_active = -1; }
    void deletePilot(int index);
    bool resurrect(int index);
    bool rename(int index, const std::string& name);

    // theaters of the active pilot
    int openTheater(Pilot& p, const std::string& def, int objectivesTotal, int planes);   // index, -1 if the pilot has too many
    void deleteTheater(Pilot& p, int index);
    std::string theaterFile(const TheaterRecord& t) const;   // the universe copy of this theater

    // A flown mission: the pilot's records, the lists, the rank and the medals. Returns the medals won.
    struct Outcome {
        MissionScore score;
        bool recorded = false;
        std::vector<int> newMedals;
        bool promoted = false;
        int oldRank = 0, newRank = 0;
        bool newHighScore = false;
    };
    Outcome recordMission(Pilot& p, TheaterRecord& t, const ResultBlock& r, long flightSeconds, int objectivesRemaining);

    const std::vector<ScoreEntry>& list(ScoreList which) const;
    void clearList(ScoreList which);

    // the theaters that have to be completed to become a Combat Master
    void setRequiredTheaters(std::vector<std::string> names) { m_required = std::move(names); }

    static std::string today(int daysAhead = 0);

private:
    void updateLists(const Pilot& p, const TheaterRecord* t, const MissionScore& s);
    void freezeCareer(const Pilot& p);
    std::string m_dir;
    std::vector<Pilot> m_pilots;
    int m_active = -1;
    int m_nextPilotId = 1, m_nextTheaterId = 1;
    std::vector<ScoreEntry> m_high, m_top, m_fast;
    std::vector<std::string> m_required;
};

} // namespace corncob
