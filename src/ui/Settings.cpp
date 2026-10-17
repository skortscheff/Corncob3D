// Settings.cpp - The choices the menus keep between runs (see Settings.hpp)

#include "Settings.hpp"
#include <SDL.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace corncob {

namespace {
std::string g_saveDir;

std::string withSeparator(std::string s) {
    if (!s.empty() && s.back() != '/' && s.back() != '\\') s += '/';
    return s;
}
} // namespace

void setSaveDirectory(const std::string& dir) {
    g_saveDir = withSeparator(dir);
    std::error_code ec;
    std::filesystem::create_directories(g_saveDir, ec);
}

std::string saveDirectory() {
    if (g_saveDir.empty()) {
        if (const char* env = std::getenv("CORNCOB_SAVE")) {
            setSaveDirectory(env);
        } else if (char* pref = SDL_GetPrefPath("PieInTheSky", "Corncob3D")) {
            setSaveDirectory(pref);
            SDL_free(pref);
        } else {
            setSaveDirectory("save");
        }
    }
    return g_saveDir;
}

bool loadSettings(Settings& s) {
    std::ifstream in(saveDirectory() + "settings.cfg");
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string key;
        if (!(ls >> key)) continue;
        std::string rest;
        std::getline(ls, rest);
        if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
        const int v = std::atoi(rest.c_str());
        if (key == "music") s.music = v != 0;
        else if (key == "joystick") s.joystick = v != 0;
        else if (key == "invertPitch") s.invertPitch = v != 0;
        else if (key == "manualCentering") s.manualCentering = v != 0;
        else if (key == "forceRecenter") s.forceRecenter = v != 0;
        else if (key == "comments") s.comments = v != 0;
        else if (key == "beginnerModel") s.beginnerModel = v != 0;
        else if (key == "invulnerableCombat") s.invulnerableCombat = v != 0;
        else if (key == "trainKla") s.training.kla = v;
        else if (key == "trainAaa") s.training.aaa = v;
        else if (key == "trainVehicles") s.training.vehicles = v;
        else if (key == "trainKlaWicked") s.training.klaWicked = v;
        else if (key == "trainAaaWicked") s.training.aaaWicked = v;
        else if (key == "trainVehWicked") s.training.vehWicked = v;
        else if (key == "trainInvulnerable") s.training.invulnerable = v != 0;
        else if (key == "activePilot") s.activePilot = rest;
    }
    return true;
}

bool saveSettings(const Settings& s) {
    std::ofstream out(saveDirectory() + "settings.cfg");
    if (!out) return false;
    out << "music " << s.music << "\njoystick " << s.joystick << "\ninvertPitch " << s.invertPitch << "\nmanualCentering " << s.manualCentering << "\nforceRecenter "
        << s.forceRecenter << "\ncomments " << s.comments << "\nbeginnerModel " << s.beginnerModel << "\ninvulnerableCombat "
        << s.invulnerableCombat << "\ntrainKla " << s.training.kla << "\ntrainAaa " << s.training.aaa << "\ntrainVehicles "
        << s.training.vehicles << "\ntrainKlaWicked " << s.training.klaWicked << "\ntrainAaaWicked " << s.training.aaaWicked
        << "\ntrainVehWicked " << s.training.vehWicked << "\ntrainInvulnerable " << s.training.invulnerable << "\nactivePilot "
        << s.activePilot << "\n";
    return static_cast<bool>(out);
}

} // namespace corncob
