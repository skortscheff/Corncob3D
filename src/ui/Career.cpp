// Career.cpp - The pilots, their theaters of operation and the score lists (see Career.hpp)

#include "Career.hpp"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <sstream>

namespace corncob {

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// a line of fields separated by '|'
std::vector<std::string> split(const std::string& line, char sep = '|') {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

long num(const std::vector<std::string>& f, size_t i) { return i < f.size() ? std::atol(f[i].c_str()) : 0; }

std::string tallyText(const Tally& t) {
    std::ostringstream s;
    s << t.objectives << ' ' << t.fuelDumps << ' ' << t.aaa << ' ' << t.sites << ' ' << t.vehicles << ' ' << t.missiles;
    return s.str();
}

Tally tallyFrom(const std::string& s) {
    std::istringstream in(s);
    Tally t;
    in >> t.objectives >> t.fuelDumps >> t.aaa >> t.sites >> t.vehicles >> t.missiles;
    return t;
}

std::string statsText(const Stats& s) {
    std::ostringstream o;
    o << s.missions << '|' << s.points << '|' << s.best << '|' << tallyText(s.kills) << '|' << tallyText(s.bestKills) << '|' << s.planesLost
      << '|' << s.friendliesLost << '|' << s.damage << '|' << s.flightSeconds << '|' << s.survived << '|' << s.missionsWithoutLoss;
    return o.str();
}

Stats statsFrom(const std::vector<std::string>& f, size_t at) {
    Stats s;
    s.missions = static_cast<int>(num(f, at));
    s.points = num(f, at + 1);
    s.best = num(f, at + 2);
    s.kills = tallyFrom(f.size() > at + 3 ? f[at + 3] : "");
    s.bestKills = tallyFrom(f.size() > at + 4 ? f[at + 4] : "");
    s.planesLost = static_cast<int>(num(f, at + 5));
    s.friendliesLost = static_cast<int>(num(f, at + 6));
    s.damage = num(f, at + 7);
    s.flightSeconds = num(f, at + 8);
    s.survived = static_cast<int>(num(f, at + 9));
    s.missionsWithoutLoss = static_cast<int>(num(f, at + 10));
    return s;
}

} // namespace

Career::Career(const std::string& dir) : m_dir(dir) {}

std::string Career::today(int daysAhead) {
    std::time_t t = std::time(nullptr) + static_cast<std::time_t>(daysAhead) * 86400;
    char buf[32];
    std::strftime(buf, sizeof buf, "%b %d, %Y", std::localtime(&t));
    return buf;
}

std::string Career::theaterFile(const TheaterRecord& t) const { return m_dir + "theater" + std::to_string(t.id) + ".cct"; }

// ---- files ----
bool Career::save() const {
    std::ofstream out(m_dir + "career.dat");
    if (!out) return false;
    out << "CORNCOB-CAREER 1\n";
    out << "next|" << m_nextPilotId << '|' << m_nextTheaterId << '|' << m_active << '\n';
    for (const Pilot& p : m_pilots) {
        out << "pilot|" << p.id << '|' << p.name << '|' << static_cast<int>(p.status) << '|' << p.releaseDate << '|' << p.resurrections
            << '|' << p.medals << '|' << p.rank << '|' << p.current << '|' << p.bestScoreTheater << '|' << statsText(p.career) << '\n';
        std::string completed;
        for (const std::string& c : p.completed) completed += (completed.empty() ? "" : ",") + c;
        out << "completed|" << completed << '\n';
        for (const TheaterRecord& t : p.theaters)
            out << "theater|" << t.id << '|' << t.def << '|' << t.objectivesTotal << '|' << t.objectivesRemaining << '|' << t.complete << '|'
                << t.startTower << '|' << t.planesLeft << '|' << t.storagePlanes << '|' << statsText(t.stats) << '\n';
    }
    auto writeList = [&](const char* tag, const std::vector<ScoreEntry>& list) {
        for (const ScoreEntry& e : list)
            out << tag << '|' << e.pilotId << '|' << e.name << '|' << e.deleted << '|' << e.score << '|' << e.theater << '|' << e.missions << '|'
                << e.resurrections << '|' << e.objectives << '|' << e.primary << '|' << e.other << '|' << e.average << '\n';
    };
    writeList("high", m_high);
    writeList("top", m_top);
    writeList("fast", m_fast);
    return static_cast<bool>(out);
}

bool Career::load() {
    std::ifstream in(m_dir + "career.dat");
    if (!in) return false;
    std::string line;
    std::getline(in, line);
    if (line.rfind("CORNCOB-CAREER", 0) != 0) return false;
    m_pilots.clear();
    m_high.clear();
    m_top.clear();
    m_fast.clear();
    while (std::getline(in, line)) {
        const std::vector<std::string> f = split(line);
        if (f.empty()) continue;
        if (f[0] == "next") {
            m_nextPilotId = static_cast<int>(num(f, 1));
            m_nextTheaterId = static_cast<int>(num(f, 2));
            m_active = static_cast<int>(num(f, 3));
        } else if (f[0] == "pilot") {
            Pilot p;
            p.id = static_cast<int>(num(f, 1));
            p.name = f.size() > 2 ? f[2] : "";
            p.status = static_cast<PilotStatus>(num(f, 3));
            p.releaseDate = f.size() > 4 ? f[4] : "";
            p.resurrections = static_cast<int>(num(f, 5));
            p.medals = static_cast<unsigned>(num(f, 6));
            p.rank = static_cast<int>(num(f, 7));
            p.current = static_cast<int>(num(f, 8));
            p.bestScoreTheater = static_cast<int>(num(f, 9));
            p.career = statsFrom(f, 10);
            m_pilots.push_back(std::move(p));
        } else if (f[0] == "completed" && !m_pilots.empty()) {
            if (f.size() > 1) m_pilots.back().completed = split(f[1], ',');
            m_pilots.back().completed.erase(std::remove(m_pilots.back().completed.begin(), m_pilots.back().completed.end(), std::string()),
                                            m_pilots.back().completed.end());
        } else if (f[0] == "theater" && !m_pilots.empty()) {
            TheaterRecord t;
            t.id = static_cast<int>(num(f, 1));
            t.def = f.size() > 2 ? f[2] : "";
            t.objectivesTotal = static_cast<int>(num(f, 3));
            t.objectivesRemaining = static_cast<int>(num(f, 4));
            t.complete = num(f, 5) != 0;
            t.startTower = static_cast<int>(num(f, 6));
            t.planesLeft = static_cast<int>(num(f, 7));
            t.storagePlanes = static_cast<int>(num(f, 8));
            t.stats = statsFrom(f, 9);
            m_pilots.back().theaters.push_back(std::move(t));
        } else if (f[0] == "high" || f[0] == "top" || f[0] == "fast") {
            ScoreEntry e;
            e.pilotId = static_cast<int>(num(f, 1));
            e.name = f.size() > 2 ? f[2] : "";
            e.deleted = num(f, 3) != 0;
            e.score = num(f, 4);
            e.theater = f.size() > 5 ? f[5] : "";
            e.missions = static_cast<int>(num(f, 6));
            e.resurrections = static_cast<int>(num(f, 7));
            e.objectives = static_cast<int>(num(f, 8));
            e.primary = static_cast<int>(num(f, 9));
            e.other = static_cast<int>(num(f, 10));
            e.average = num(f, 11);
            (f[0] == "high" ? m_high : f[0] == "top" ? m_top : m_fast).push_back(std::move(e));
        }
    }
    if (m_active >= static_cast<int>(m_pilots.size())) m_active = -1;
    return true;
}

// ---- pilots ----
int Career::findPilot(const std::string& name) const {
    for (size_t i = 0; i < m_pilots.size(); i++)
        if (lower(m_pilots[i].name) == lower(name)) return static_cast<int>(i);
    return -1;
}

int Career::createPilot(const std::string& name) {
    if (name.empty() || static_cast<int>(m_pilots.size()) >= MAX_PILOTS || findPilot(name) >= 0) return -1;
    Pilot p;
    p.id = m_nextPilotId++;
    p.name = name.substr(0, MAX_NAME);
    m_pilots.push_back(std::move(p));
    return static_cast<int>(m_pilots.size()) - 1;
}

bool Career::activate(int index) {
    if (index < 0 || index >= static_cast<int>(m_pilots.size())) return false;
    if (m_pilots[index].dead()) return false;
    m_active = index;
    return true;
}

void Career::freezeCareer(const Pilot& p) {
    for (auto* list : {&m_high, &m_top, &m_fast})
        for (ScoreEntry& e : *list)
            if (e.pilotId == p.id) e.deleted = true;
}

void Career::deletePilot(int index) {
    if (index < 0 || index >= static_cast<int>(m_pilots.size())) return;
    freezeCareer(m_pilots[index]);
    for (const TheaterRecord& t : m_pilots[index].theaters) std::remove(theaterFile(t).c_str());
    m_pilots.erase(m_pilots.begin() + index);
    if (m_active == index) m_active = -1;
    else if (m_active > index) m_active--;
}

bool Career::resurrect(int index) {
    if (index < 0 || index >= static_cast<int>(m_pilots.size()) || !m_pilots[index].dead()) return false;
    m_pilots[index].status = PilotStatus::Ready;
    m_pilots[index].releaseDate.clear();
    m_pilots[index].resurrections++;
    return true;
}

bool Career::rename(int index, const std::string& name) {
    if (index < 0 || index >= static_cast<int>(m_pilots.size()) || name.empty()) return false;
    const int other = findPilot(name);
    if (other >= 0 && other != index) return false;
    m_pilots[index].name = name.substr(0, MAX_NAME);
    for (auto* list : {&m_high, &m_top, &m_fast})
        for (ScoreEntry& e : *list)
            if (e.pilotId == m_pilots[index].id) e.name = m_pilots[index].name;
    return true;
}

// ---- theaters ----
int Career::openTheater(Pilot& p, const std::string& def, int objectivesTotal, int planes) {
    if (static_cast<int>(p.theaters.size()) >= MAX_THEATERS) return -1;
    TheaterRecord t;
    t.id = m_nextTheaterId++;
    t.def = def;
    t.objectivesTotal = t.objectivesRemaining = objectivesTotal;
    t.planesLeft = planes;
    p.theaters.push_back(t);
    p.current = static_cast<int>(p.theaters.size()) - 1;
    return p.current;
}

void Career::deleteTheater(Pilot& p, int index) {
    if (index < 0 || index >= static_cast<int>(p.theaters.size())) return;
    std::remove(theaterFile(p.theaters[index]).c_str());
    p.theaters.erase(p.theaters.begin() + index);
    if (p.current == index) p.current = p.theaters.empty() ? -1 : 0;
    else if (p.current > index) p.current--;
}

// ---- missions ----
void Career::updateLists(const Pilot& p, const TheaterRecord* t, const MissionScore& s) {
    auto sortAndCut = [](std::vector<ScoreEntry>& list, auto less) {
        std::stable_sort(list.begin(), list.end(), less);
        if (list.size() > 10) list.resize(10);
    };
    if (t && s.counts && s.total > 0) {
        ScoreEntry e;
        e.pilotId = p.id;
        e.name = p.name;
        e.score = s.total;
        e.theater = t->def;
        e.objectives = s.kills.objectives;
        e.primary = s.kills.primary();
        e.other = s.kills.other();
        m_high.push_back(e);
        sortAndCut(m_high, [](const ScoreEntry& a, const ScoreEntry& b) { return a.score > b.score; });
    }
    auto careerEntry = [&](std::vector<ScoreEntry>& list) -> ScoreEntry& {
        for (ScoreEntry& e : list)
            if (e.pilotId == p.id) return e;
        list.push_back(ScoreEntry());
        return list.back();
    };
    auto fill = [&](ScoreEntry& e) {
        e.pilotId = p.id;
        e.name = p.name;
        e.deleted = false;
        e.score = p.career.points;
        e.missions = p.career.missions;
        e.resurrections = p.resurrections;
        e.objectives = p.career.kills.objectives;
        e.primary = p.career.kills.primary();
        e.other = p.career.kills.other();
        e.average = p.career.missions > 0 ? p.career.points / p.career.missions : 0;
    };
    if (p.career.missions > 0) {
        fill(careerEntry(m_top));
        sortAndCut(m_top, [](const ScoreEntry& a, const ScoreEntry& b) { return a.score > b.score; });
        if (p.career.missions >= 3) {
            fill(careerEntry(m_fast));
            sortAndCut(m_fast, [](const ScoreEntry& a, const ScoreEntry& b) { return a.average > b.average; });
        }
    }
}

Career::Outcome Career::recordMission(Pilot& p, TheaterRecord& t, const ResultBlock& r, long flightSeconds, int objectivesRemaining) {
    Outcome out;
    out.oldRank = p.rank;
    out.score = scoreMission(r);
    out.recorded = out.score.counts;
    const bool dead = (r.status & ST_KILLED) || (r.status & ST_CAPTURED);
    if (!out.recorded) return out;
    for (Stats* s : {&p.career, &t.stats}) {
        s->missions++;
        s->points += out.score.total;
        s->kills.add(out.score.kills);
        if (out.score.total > s->best) {
            s->best = out.score.total;
            s->bestKills = out.score.kills;
        }
        s->planesLost += r.planesLost;
        s->friendliesLost += r.friendliesLost;
        s->damage += r.maxDamage;
        s->flightSeconds += flightSeconds;
        if (!dead) s->survived++;
        if (r.planesLost == 0) s->missionsWithoutLoss++;
    }
    out.newHighScore = !m_high.empty() ? out.score.total > m_high.back().score || m_high.size() < 10 : true;
    // the plane count of the theater
    t.planesLeft = std::max(0, t.planesLeft - static_cast<int>(r.planesLost));
    t.objectivesRemaining = std::max(0, objectivesRemaining);
    t.complete = t.objectivesTotal > 0 && t.objectivesRemaining == 0;
    if (t.complete && std::find(p.completed.begin(), p.completed.end(), t.def) == p.completed.end()) p.completed.push_back(t.def);
    // the pilot's fate
    if (r.status & ST_KILLED) p.status = PilotStatus::Killed;
    if (r.status & ST_CAPTURED) {
        p.status = PilotStatus::Stockade;
        p.releaseDate = today(7);
    }
    if (dead && m_active >= 0 && &m_pilots[m_active] == &p) m_active = -1;
    // the medals
    auto award = [&](int medal) {
        if (!(p.medals & (1u << medal))) {
            p.medals |= 1u << medal;
            out.newMedals.push_back(medal);
        }
    };
    const long raw = out.score.raw;
    if (!dead && r.maxDamage >= 150) award(PURPLE_HEART);
    if (raw >= 3000) award(BRONZE_STAR);
    if (raw >= 8000) award(SILVER_STAR);
    if (raw >= 20000) award(GOLD_STAR);
    if (raw >= 60000 && out.score.landing == 5) award(STELLAR_CLUSTER);
    if (p.career.kills.objectives >= 10) award(DSC);
    if (p.career.survived >= 20) award(SERVICE_AWARD);
    if (p.career.missionsWithoutLoss >= 10) award(DFC);
    if (t.complete) award(THEATER_MEDAL);
    bool all = !m_required.empty();
    for (const std::string& need : m_required)
        if (std::find(p.completed.begin(), p.completed.end(), need) == p.completed.end()) all = false;
    if (all) award(COMBAT_MASTER);
    p.rank = std::max(p.rank, rankForPoints(p.career.points));
    out.newRank = p.rank;
    out.promoted = p.rank > out.oldRank;
    updateLists(p, &t, out.score);
    return out;
}

const std::vector<ScoreEntry>& Career::list(ScoreList which) const {
    return which == ScoreList::HighScores ? m_high : which == ScoreList::TopCareers ? m_top : m_fast;
}

void Career::clearList(ScoreList which) {
    (which == ScoreList::HighScores ? m_high : which == ScoreList::TopCareers ? m_top : m_fast).clear();
}

} // namespace corncob
