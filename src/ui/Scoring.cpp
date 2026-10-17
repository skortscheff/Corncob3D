// Scoring.cpp - What a mission was worth (see Scoring.hpp)

#include "Scoring.hpp"
#include <algorithm>
#include <cstring>

namespace corncob {

namespace {

// the order of the words in the block: missionstatus, deathcode, nplaneslost, ndest, 4 x 8 words of kills by
// quality, nhqsdest, nfdest, maxtotaldamage, 2 x 8 words of "on me", etime, elapsedtime, 8 words of orbs
struct Cursor {
    const std::vector<uint8_t>& b;
    size_t pos = 0;
    uint16_t next() {
        const uint16_t v = pos + 1 < b.size() ? static_cast<uint16_t>(b[pos] | (b[pos + 1] << 8)) : 0;
        pos += 2;
        return v;
    }
};

} // namespace

bool ResultBlock::parse(const std::vector<uint8_t>& block, ResultBlock& out) {
    if (block.size() < 0x82) return false;   // up to and including the orb table
    Cursor c{block};
    out.status = c.next();
    out.deathcode = c.next();
    out.planesLost = c.next();
    out.fuelDumps = c.next();
    for (int k = 0; k < 8; k++) out.klaDestroyed[k] = c.next();
    for (int k = 0; k < 8; k++) out.aaaDestroyed[k] = c.next();
    for (int k = 0; k < 8; k++) out.vehiclesDestroyed[k] = c.next();
    for (int k = 0; k < 8; k++) out.sitesDestroyed[k] = c.next();
    out.objectives = c.next();
    out.friendliesLost = c.next();
    out.maxDamage = c.next();
    for (int k = 0; k < 8; k++) out.maxAaOnMe[k] = c.next();
    for (int k = 0; k < 8; k++) out.maxKlaOnMe[k] = c.next();
    out.flightTicks = c.next();
    out.elapsed = c.next();
    for (int k = 0; k < 8; k++) out.orbsDestroyed[k] = c.next();
    return true;
}

std::vector<uint8_t> ResultBlock::serialize() const {
    std::vector<uint8_t> b;
    auto put = [&](uint16_t v) {
        b.push_back(static_cast<uint8_t>(v & 0xFF));
        b.push_back(static_cast<uint8_t>(v >> 8));
    };
    put(status); put(deathcode); put(planesLost); put(fuelDumps);
    for (uint16_t v : klaDestroyed) put(v);
    for (uint16_t v : aaaDestroyed) put(v);
    for (uint16_t v : vehiclesDestroyed) put(v);
    for (uint16_t v : sitesDestroyed) put(v);
    put(objectives); put(friendliesLost); put(maxDamage);
    for (uint16_t v : maxAaOnMe) put(v);
    for (uint16_t v : maxKlaOnMe) put(v);
    put(flightTicks); put(elapsed);
    for (uint16_t v : orbsDestroyed) put(v);
    return b;
}

void Tally::add(const Tally& t) {
    objectives += t.objectives;
    fuelDumps += t.fuelDumps;
    aaa += t.aaa;
    sites += t.sites;
    vehicles += t.vehicles;
    missiles += t.missiles;
}

namespace {
int sum(const uint16_t (&a)[8]) {
    int s = 0;
    for (uint16_t v : a) s += v;
    return s;
}
long weighted(const uint16_t (&a)[8], int base, int perQuality) {
    long s = 0;
    for (int q = 0; q < 8; q++) s += static_cast<long>(a[q]) * (base + perQuality * q);
    return s;
}
} // namespace

Tally talliesOf(const ResultBlock& r) {
    Tally t;
    t.objectives = r.objectives;
    t.fuelDumps = r.fuelDumps;
    t.aaa = sum(r.aaaDestroyed);
    t.sites = sum(r.sitesDestroyed) + sum(r.orbsDestroyed);
    t.vehicles = sum(r.vehiclesDestroyed);
    t.missiles = sum(r.klaDestroyed);
    return t;
}

MissionScore scoreMission(const ResultBlock& r) {
    MissionScore s;
    s.kills = talliesOf(r);
    // the raw score: what was destroyed, the better defences worth more
    s.raw = 1000L * r.objectives + 100L * r.fuelDumps + weighted(r.aaaDestroyed, 40, 20) + weighted(r.sitesDestroyed, 100, 50) +
            weighted(r.orbsDestroyed, 300, 100) + weighted(r.vehiclesDestroyed, 10, 10) + weighted(r.klaDestroyed, 5, 5);
    const uint16_t st = r.status;
    if (st & (ST_INVINCIBLE | ST_ABORT_CTRL_C)) s.counts = false;   // the mission does not happen
    const bool homeSafe = (st & ST_HOME) && !(st & (ST_KILLED | ST_CAPTURED));
    if (st & ST_RESCUED) s.home = 4;
    else if (homeSafe) s.home = 5;
    else if (st & ST_CRASH_HOME) s.home = 3;
    else if (st & ST_EJECTED) s.home = 2;
    else s.home = 1;
    s.landing = (st & ST_LANDED_HOME) && (st & ST_HOME) && !(st & (ST_CRASH_HOME | ST_CRASH_AWAY)) ? 5 : 1;
    s.worst = 1.0;
    if (st & ST_RESCUED) s.worst = 0.5;
    if (st & ST_EJECTED) s.worst = std::min(s.worst, 0.5);
    if (st & (ST_CRASH_AWAY | ST_CRASH_HOME)) s.worst = std::min(s.worst, 0.5);
    if (st & ST_HIT_BUILDING) s.worst = std::min(s.worst, 0.75);
    if (st & (ST_KILLED | ST_CAPTURED)) s.worst = std::min(s.worst, 0.25);
    s.total = static_cast<long>(static_cast<double>(s.raw) * s.home * s.landing * s.worst);
    if (!s.counts) s.total = 0;
    return s;
}

std::vector<std::string> statusWords(uint16_t st) {
    std::vector<std::string> w;
    if (st & ST_ABORT_AIR) w.push_back("MISSION-ABORTED");
    if (st & ST_KILLED) w.push_back("KILLED");
    if (st & ST_CAPTURED) w.push_back("CAPTURED");
    if (st & ST_HOME) w.push_back("AT-HOME");
    if (st & ST_RESCUED) w.push_back("RESCUED");
    if (st & ST_CRASH_AWAY) w.push_back("CRASHED-NOT-HOME");
    if (st & ST_EJECTED) w.push_back("EJECTED");
    if (st & ST_CRASH_HOME) w.push_back("CRASHED-AT-HOME");
    if (st & ST_LANDED_HOME) w.push_back("LANDED-AT-HOME");
    if (st & ST_HIT_BUILDING) w.push_back("COLLISION");
    if (st & ST_SUICIDE) w.push_back("SUICIDE");
    return w;
}

const char* rankName(int rank, bool abbreviated) {
    static const char* full[NRANKS] = {"2nd Lieutenant", "1st Lieutenant", "Captain", "Major", "Lt. Colonel", "General"};
    static const char* abbr[NRANKS] = {"2nd Lt.", "1st Lt.", "Capt.", "Major", "Lt. Col.", "General"};
    rank = std::max(0, std::min(NRANKS - 1, rank));
    return abbreviated ? abbr[rank] : full[rank];
}

int rankForPoints(long p) {
    static const long thresholds[NRANKS] = {0, 50000, 200000, 600000, 1500000, 4000000};
    int rank = 0;
    for (int i = 0; i < NRANKS; i++)
        if (p >= thresholds[i]) rank = i;
    return rank;
}

const char* medalName(int m) {
    static const char* names[NMEDALS] = {"Purple Heart", "Bronze Star", "Silver Star", "Gold Star", "Distinguished Service Cross",
                                         "Service Award", "Stellar Cluster", "Combat Master", "Distinguished Flying Cross", "Theater Combat Medal"};
    return m >= 0 && m < NMEDALS ? names[m] : "";
}

const char* medalCitation(int m) {
    static const char* text[NMEDALS] = {"surviving extreme injury in combat", "inflicting heavy damage to the enemy",
                                        "inflicting severe damage to the enemy", "inflicting extreme damage to the enemy",
                                        "extraordinary accomplishment of objectives", "surviving numerous combat sorties",
                                        "inflicting catastrophic damage to the enemy", "Completely obliterating the Alien threat",
                                        "extraordinary services without losing a plane", "completing all mission objectives within a T-of-O"};
    return m >= 0 && m < NMEDALS ? text[m] : "";
}

} // namespace corncob
