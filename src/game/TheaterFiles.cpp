// TheaterFiles.cpp - Reading the theater files without loading the game (see TheaterFiles.hpp)

#include "TheaterFiles.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace corncob {

namespace {

constexpr int NTILES = 64, HEADER = 16, OBJ = 23;

uint16_t w(const std::vector<uint8_t>& b, size_t at) {
    return at + 1 < b.size() ? static_cast<uint16_t>(b[at] | (b[at + 1] << 8)) : 0;
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string clean(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    std::string out;
    for (char c : s) out += (c >= 32 && c < 127) ? c : (c == '\t' ? ' ' : '\0');
    out.erase(std::remove(out.begin(), out.end(), '\0'), out.end());
    return out;
}

} // namespace

bool summarizeUniverse(const std::vector<uint8_t>& cct, UniverseSummary& out) {
    out = UniverseSummary();
    size_t pos = 0;
    for (int t = 0; t < NTILES; t++) {
        if (pos + HEADER > cct.size() || cct[pos] != 'P' || cct[pos + 1] != 'i') return false;
        const int count = w(cct, pos + 8);
        pos += HEADER;
        if (pos + static_cast<size_t>(count) * OBJ > cct.size()) return false;
        for (int k = 0; k < count; k++, pos += OBJ) {
            const uint16_t type = w(cct, pos + 11);
            if (type == 0) continue;
            // the template index is stored xor'ed with the x and y words
            const int tmpl = w(cct, pos + 21) ^ w(cct, pos + 4) ^ w(cct, pos + 2);
            const int quality = (type >> 12) & 7;
            const bool done = (type & 0x200) != 0;
            bool objective = false;
            switch (tmpl) {
            case 0: case 43: case 53: objective = true; break;           // an orb (alive or wrecked), a mission saucer
            case 7: objective = (type & 0x800) != 0; break;              // a spy
            case 58: objective = quality == 4 || quality == 7; break;    // a portal that finishes the mission
            case 2:                                                      // a plane
                if (type & 0x100) out.crashedPlanes++;
                else if (type & 0x1000) out.brownPlanes++;
                else if (!(type & 0x2000)) out.planes++;
                break;
            default: break;
            }
            if (objective && !done) out.objectivesRemaining++;
        }
    }
    if (pos + 12 > cct.size()) return false;
    out.startX = static_cast<int32_t>(w(cct, pos) | (static_cast<uint32_t>(w(cct, pos + 2)) << 16));
    out.startY = static_cast<int32_t>(w(cct, pos + 4) | (static_cast<uint32_t>(w(cct, pos + 6)) << 16));
    out.startZ = static_cast<int32_t>(w(cct, pos + 8) | (static_cast<uint32_t>(w(cct, pos + 10)) << 16));
    return true;
}

bool loadTheaterInfo(const std::string& twrPath, TheaterInfo& out) {
    out = TheaterInfo();
    std::ifstream in(twrPath, std::ios::binary);
    if (!in) return false;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) lines.push_back(clean(line));
    size_t i = 0;
    if (i < lines.size() && !lines[i].empty() && lines[i][0] == 'X') i++;   // the parameter line
    if (i < lines.size()) out.name = lines[i++];
    for (int k = 0; k < 9 && i < lines.size(); k++, i++)
        if (lines[i] != "." && !lines[i].empty()) out.description.push_back(lines[i]);
    std::vector<std::string>* text = &out.airfieldBanner;
    MissionInfo* mission = nullptr;
    for (; i < lines.size(); i++) {
        const std::string& l = lines[i];
        const size_t welcome = l.find("Welcome to ");
        if (welcome != std::string::npos && out.airfield.empty()) {
            std::string name = l.substr(welcome + 11);
            name = name.substr(0, name.find(" -"));
            name.erase(std::remove(name.begin(), name.end(), '^'), name.end());   // ^ breaks the line on the screen
            out.airfield = name;
        }
        if (l.find("Mission Code Name :") != std::string::npos) {
            out.missions.emplace_back();
            mission = &out.missions.back();
            std::string n = l.substr(l.find(':') + 1);
            while (!n.empty() && n.front() == ' ') n.erase(n.begin());
            while (!n.empty() && n.back() == ' ') n.pop_back();
            mission->codeName = n;
            text = &mission->briefing;
            continue;
        }
        if (mission && l.rfind(" Direction", 0) == 0) { mission->direction = l.substr(l.find(':') + 2); continue; }
        if (mission && l.rfind(" Distance", 0) == 0) { mission->distance = l.substr(l.find(':') + 2); continue; }
        if (mission && l.rfind(" Difficulty", 0) == 0) { mission->difficulty = l.substr(l.find(':') + 2); continue; }
        if (mission && l.rfind("Sxyth ", 0) == 0) {   // where the mission starts
            long v[4] = {0, 0, 0, 0};
            const char* p = l.c_str() + 6;
            for (int k = 0; k < 4; k++) {
                char* end = nullptr;
                v[k] = std::strtol(p, &end, 10);
                p = end;
            }
            mission->x = static_cast<int32_t>(v[0]);
            mission->y = static_cast<int32_t>(v[1]);
            mission->z = static_cast<int32_t>(v[2]);
            mission->yaw = static_cast<int32_t>(v[3]);
            continue;
        }
        if (text) text->push_back(l);
    }
    return true;
}

std::vector<std::string> listTheaters(const std::string& defDir) {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(defDir, ec)) {
        const std::string ext = upper(e.path().extension().string());
        if (ext != ".CCT") continue;
        const std::string stem = e.path().stem().string();
        if (std::filesystem::exists(e.path().parent_path() / (stem + ".TWR"), ec) ||
            std::filesystem::exists(e.path().parent_path() / (stem + ".twr"), ec))
            names.push_back(stem);
    }
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) { return upper(a) < upper(b); });
    return names;
}

} // namespace corncob
