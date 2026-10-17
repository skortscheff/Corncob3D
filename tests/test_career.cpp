// test_career.cpp - Scoring (CORNCOB.DOC's examples), pilots, theaters, medals, the score lists and the files

#include "ui/Career.hpp"
#include "ui/Scoring.hpp"
#include <cstdio>
#include <filesystem>

using namespace corncob;

static int failures = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static ResultBlock block(uint16_t status) {
    ResultBlock r;
    r.status = status;
    r.objectives = 1;   // 1000 raw
    return r;
}

int main() {
    // the examples of the manual: a raw score of 1000
    {
        const MissionScore landed = scoreMission(block(ST_HOME | ST_LANDED_HOME));
        CHECK(landed.raw == 1000);
        CHECK(landed.total == 25000);   // 5 * 5 * 1 * 1000
        const MissionScore rescued = scoreMission(block(ST_RESCUED | ST_EJECTED));
        CHECK(rescued.total == 2000);   // 4 * 1 * 1/2 * 1000
        const MissionScore flyby = scoreMission(block(ST_HOME));
        CHECK(flyby.total == 5000);     // 5 * 1 * 1 * 1000
        CHECK(!scoreMission(block(ST_HOME | ST_INVINCIBLE)).counts);   // an invulnerable plane doesn't count
        CHECK(!scoreMission(block(ST_ABORT_CTRL_C)).counts);
    }

    // the block of 3.EXE
    {
        ResultBlock r = block(ST_HOME);
        r.aaaDestroyed[3] = 2;
        r.sitesDestroyed[1] = 1;
        r.klaDestroyed[0] = 5;
        r.orbsDestroyed[2] = 1;
        const std::vector<uint8_t> bytes = r.serialize();
        CHECK(bytes.size() == 0x82);
        ResultBlock back;
        CHECK(ResultBlock::parse(bytes, back));
        CHECK(back.status == ST_HOME && back.objectives == 1 && back.aaaDestroyed[3] == 2 && back.orbsDestroyed[2] == 1);
        const Tally t = talliesOf(back);
        CHECK(t.aaa == 2 && t.sites == 2 && t.missiles == 5 && t.objectives == 1);
    }

    // ranks
    CHECK(rankForPoints(0) == 0);
    CHECK(rankForPoints(60000) == 1);
    CHECK(rankForPoints(10000000) == 5);
    CHECK(std::string(rankName(0, false)) == "2nd Lieutenant" && std::string(rankName(4, true)) == "Lt. Col.");

    // pilots
    const std::string dir = (std::filesystem::temp_directory_path() / "corncob_test_career/").string();
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        Career c(dir);
        CHECK(!c.load());   // nothing yet
        const int a = c.createPilot("Ace");
        const int b = c.createPilot("Baker");
        CHECK(a == 0 && b == 1);
        CHECK(c.createPilot("ace") < 0);   // the name is taken (not case sensitive)
        CHECK(c.activate(a));
        CHECK(c.active()->name == "Ace");

        Pilot& ace = c.pilots()[a];
        const int th = c.openTheater(ace, "MORNING", 3, 2);
        CHECK(th == 0 && ace.current == 0);
        ResultBlock r = block(ST_HOME | ST_LANDED_HOME);
        r.fuelDumps = 25;   // raw 1000 + 2500 = 3500
        const Career::Outcome o = c.recordMission(ace, ace.theaters[0], r, 600, 2);
        CHECK(o.recorded && o.score.total == 25 * 3500);
        CHECK(ace.career.missions == 1 && ace.career.points == 87500 && ace.theaters[0].stats.points == 87500);
        CHECK(ace.theaters[0].objectivesRemaining == 2 && !ace.theaters[0].complete);
        CHECK(o.promoted && ace.rank == 1);   // 87500 points: a first lieutenant
        CHECK(c.list(ScoreList::HighScores).size() == 1 && c.list(ScoreList::HighScores)[0].score == 87500);
        CHECK(c.list(ScoreList::TopCareers).size() == 1);
        CHECK(c.list(ScoreList::FastCareers).empty());   // needs three missions
        bool bronze = false;
        for (int m : o.newMedals) bronze = bronze || m == BRONZE_STAR;
        CHECK(bronze);

        // killed in action: deactivated, can't be activated, can be resurrected
        Pilot& baker = c.pilots()[b];
        c.openTheater(baker, "DUSK", 2, 2);
        CHECK(c.activate(b));
        c.recordMission(baker, baker.theaters[0], block(ST_KILLED), 300, 2);
        CHECK(baker.status == PilotStatus::Killed && c.activeIndex() < 0);
        CHECK(!c.activate(b));
        CHECK(c.resurrect(b) && c.pilots()[b].resurrections == 1 && c.activate(b));

        // captured: the stockade
        c.recordMission(c.pilots()[b], c.pilots()[b].theaters[0], block(ST_CAPTURED), 300, 2);
        CHECK(c.pilots()[b].status == PilotStatus::Stockade && !c.pilots()[b].releaseDate.empty());
        c.resurrect(b);

        // the theater completes: the medal, and the pilot has completed it
        c.setRequiredTheaters({"DUSK"});
        ResultBlock done = block(ST_HOME);
        const Career::Outcome o2 = c.recordMission(c.pilots()[b], c.pilots()[b].theaters[0], done, 100, 0);
        CHECK(c.pilots()[b].theaters[0].complete && c.pilots()[b].completed.size() == 1);
        bool tmedal = false, master = false;
        for (int m : o2.newMedals) { tmedal = tmedal || m == THEATER_MEDAL; master = master || m == COMBAT_MASTER; }
        CHECK(tmedal && master);

        CHECK(c.save());
    }
    {   // it all comes back from the file
        Career c(dir);
        CHECK(c.load());
        CHECK(c.pilots().size() == 2 && c.pilots()[0].name == "Ace" && c.pilots()[0].career.points == 87500);
        CHECK(c.pilots()[1].resurrections == 2 && c.pilots()[1].theaters.size() == 1);
        CHECK(c.pilots()[1].completed.size() == 1 && c.pilots()[1].completed[0] == "DUSK");
        CHECK(!c.list(ScoreList::HighScores).empty());
        // deleting a pilot freezes the career in the lists
        const int id = c.pilots()[0].id;
        c.deletePilot(0);
        CHECK(c.pilots().size() == 1);
        bool frozen = false;
        for (const ScoreEntry& e : c.list(ScoreList::TopCareers)) frozen = frozen || (e.pilotId == id && e.deleted);
        CHECK(frozen);
        c.clearList(ScoreList::TopCareers);
        CHECK(c.list(ScoreList::TopCareers).empty());
        CHECK(c.rename(0, "Baker2") && c.pilots()[0].name == "Baker2");
        // many theaters
        Pilot& p = c.pilots()[0];
        int opened = 0;
        for (int i = 0; i < 20; i++) opened += c.openTheater(p, "DUSK", 1, 1) >= 0 ? 1 : 0;
        CHECK(opened + 1 == MAX_THEATERS);   // it already had one
    }
    std::filesystem::remove_all(dir);

    if (failures == 0) std::printf("test_career: all passed\n");
    return failures ? 1 : 0;
}
