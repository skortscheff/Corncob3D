// GameText.cpp - The texts of the original that the menus show (see GameText.hpp)

#include "GameText.hpp"
#include "assets/Assets.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>

namespace corncob {

namespace {

std::string readAll(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// printable text: no carriage returns or control characters (the arrows of "(\<up>\<down>...)" go, with
// their brackets)
std::string clean(const std::string& line) {
    std::string s;
    for (char c : line) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u == '\t') s += "    ";
        else if (u >= 32 && u < 127) s += c;
    }
    for (const char* arrows : {"(\\)", "(\\\\)"}) {
        size_t p;
        while ((p = s.find(arrows)) != std::string::npos) s.erase(p, std::string(arrows).size());
    }
    return s;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

} // namespace

std::vector<std::string> readTextLines(const std::string& path) {
    std::vector<std::string> out;
    const std::string all = readAll(path);
    size_t i = 0;
    while (i <= all.size()) {
        size_t j = all.find('\n', i);
        if (j == std::string::npos) j = all.size();
        if (i < all.size() || j > i) out.push_back(clean(all.substr(i, j - i)));
        i = j + 1;
    }
    return out;
}

std::vector<std::string> infoText(Info which) {
    static std::map<std::string, std::vector<std::string>> sections;
    if (sections.empty()) {
        const std::vector<std::string> lines = readTextLines(assetPath("MOAG.INF"));
        static const char* titles[] = {"Main Menu Info", "Duty Roster Menu Info", "Combat Missions Info",
                                       "OLD SHAREWARE VERSION Registration Menu Info", "Keyboard/Joystick Setup Menu Info",
                                       "Training Missions Menu.", "Hiscore Menu", "Delete a high score list menu",
                                       "NEW SHAREWARE VERSION Registration Menu Info", "Sound Editor Help"};
        std::vector<std::string>* current = nullptr;
        for (const std::string& line : lines) {
            const std::string t = trim(line);
            bool title = false;
            for (const char* name : titles)
                if (t == name) {
                    current = &sections[name];
                    title = true;
                    break;
                }
            if (!title && current) current->push_back(line);
        }
    }
    const char* key = "";
    switch (which) {
    case Info::Main: key = "Main Menu Info"; break;
    case Info::DutyRoster: key = "Duty Roster Menu Info"; break;
    case Info::Combat: key = "Combat Missions Info"; break;
    case Info::Keyboard: key = "Keyboard/Joystick Setup Menu Info"; break;
    case Info::Training: key = "Training Missions Menu."; break;
    case Info::HighScores: key = "Hiscore Menu"; break;
    case Info::DeleteList: key = "Delete a high score list menu"; break;
    case Info::ReadThis: key = "NEW SHAREWARE VERSION Registration Menu Info"; break;
    }
    std::vector<std::string> out = sections[key];
    while (!out.empty() && trim(out.front()).empty()) out.erase(out.begin());
    while (!out.empty() && trim(out.back()).empty()) out.pop_back();
    return out;
}

std::vector<std::vector<std::string>> docPages() {
    std::vector<std::vector<std::string>> pages(1);
    const std::string all = readAll(assetPath("CORNCOB.DOC"));
    std::string line;
    auto endLine = [&] {
        pages.back().push_back(clean(line));
        line.clear();
    };
    for (char c : all) {
        if (c == '\n') endLine();
        else if (c == '\f') {
            endLine();
            pages.emplace_back();
        } else line += c;
    }
    endLine();
    for (auto& p : pages) {   // trim the blank lines at the ends of a page
        while (!p.empty() && trim(p.back()).empty()) p.pop_back();
        while (!p.empty() && trim(p.front()).empty()) p.erase(p.begin());
    }
    return pages;
}

} // namespace corncob
