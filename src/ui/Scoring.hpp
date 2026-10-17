// Scoring.hpp - What a mission was worth: the result block of 3.EXE, the score, the ranks and the medals
//
// 3.EXE leaves a 140 byte block (3.res) with the mission status and what was destroyed, by quality of the
// target. TOWER/ISCORE (sourceless) turned that into the report. CORNCOB.DOC gives the shape of the score:
// a raw score for what was destroyed, multiplied by three factors that depend on how the mission ended:
//
//   landed safely at an airstrip   5 * 5 * 1 * raw
//   ejected, rescued by the van    4 * 1 * 1/2 * raw
//   a flyby of the airstrip        5 * 1 * 1 * raw
//
// The weights of the raw score and the other cases are a recreation (the original's numbers are not in
// the files), chosen so that those three examples come out as the manual shows.

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace corncob {

// The result block (the data segment from missionstatus on, little endian words)
struct ResultBlock {
    uint16_t status = 0, deathcode = 0, planesLost = 0, fuelDumps = 0;
    uint16_t klaDestroyed[8] = {}, aaaDestroyed[8] = {}, vehiclesDestroyed[8] = {}, sitesDestroyed[8] = {};
    uint16_t objectives = 0, friendliesLost = 0, maxDamage = 0;
    uint16_t maxAaOnMe[8] = {}, maxKlaOnMe[8] = {};
    uint16_t flightTicks = 0, elapsed = 0;
    uint16_t orbsDestroyed[8] = {};

    static bool parse(const std::vector<uint8_t>& block, ResultBlock& out);
    std::vector<uint8_t> serialize() const;   // the bytes for the first 0x82 of the block (for tests)
};

// What was destroyed, the way the report lists it
struct Tally {
    int objectives = 0;   // mission objectives completed
    int fuelDumps = 0;    // TRFRU and fuel dumps
    int aaa = 0;          // AAA batteries
    int sites = 0;        // other sites: missile and vehicle sites, orbs
    int vehicles = 0;     // ground transports
    int missiles = 0;     // enemy missiles (KLA)
    int primary() const { return fuelDumps + aaa + sites; }   // "primary kills" of the high score lists
    int other() const { return vehicles + missiles; }
    void add(const Tally& t);
};

Tally talliesOf(const ResultBlock& r);

enum StatusBits : uint16_t {
    ST_ABORT_AIR = 0x0001, ST_KILLED = 0x0002, ST_LANDED_HOME = 0x0004, ST_CAPTURED = 0x0008, ST_HOME = 0x0010, ST_EJECTED = 0x0020,
    ST_CRASH_AWAY = 0x0040, ST_HIT_BUILDING = 0x0080, ST_RESCUED = 0x0100, ST_CRASH_HOME = 0x0200, ST_LOST_SPACE = 0x0400,
    ST_KILLED_BY_OBJECT = 0x0800, ST_INVINCIBLE = 0x2000, ST_ABORT_CTRL_C = 0x4000, ST_SUICIDE = 0x8000
};

struct MissionScore {
    long raw = 0;
    int landing = 1;      // x5 when landed at the airstrip
    int home = 1;         // x5 at home (landed or a flyby), x4 rescued ...
    double worst = 1.0;   // 1, or less for the worst thing that happened (ejecting, crash landing ...)
    long total = 0;
    bool counts = true;   // false when the mission is not recorded (invulnerable plane, aborted)
    Tally kills;
};

MissionScore scoreMission(const ResultBlock& r);

// The words for a status, as the reports show them: "LANDED-AT-HOME", "KILLED" ... in the order of
// MOAG's table
std::vector<std::string> statusWords(uint16_t status);

// ranks and medals
constexpr int NRANKS = 6;
const char* rankName(int rank, bool abbreviated);
int rankForPoints(long careerPoints);

enum Medal { PURPLE_HEART, BRONZE_STAR, SILVER_STAR, GOLD_STAR, DSC, SERVICE_AWARD, STELLAR_CLUSTER, COMBAT_MASTER, DFC, THEATER_MEDAL, NMEDALS };
const char* medalName(int medal);
const char* medalCitation(int medal);

} // namespace corncob
