// test_demo.cpp - Replays a recorded demo (tests/demos/*.demo) and checks the plane against the recorded states.
// The simulation is deterministic, so any drift in a checkpoint means the flight model changed.
//   test_demo [file.demo]            replay and compare (default: tests/demos/takeoff.demo)
//   test_demo --record [file.demo]   replay and rewrite the `expect` lines of the file
//   CC_TRACE=<seconds> test_demo     print time, altitude, ground flag, speed and pitch for every frame instead
// Demo file lines:  theater NAME | fps N | key SECONDS down|up HEXCODE | expect SECONDS alt x y speed yaw pitch roll
// Needs the original DEF files (assets/original, never committed); skipped when missing.

#include "FlightRun.hpp"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

using namespace corncob;

struct Expect {
    double t;
    int32_t v[7];   // alt x y speed yaw pitch roll
};

static const char* const NAMES = "alt x y speed yaw pitch roll";

int main(int argc, char** argv) {
    bool record = false;
    std::string path = std::string(CORNCOB_DEMO_DIR) + "/takeoff.demo";
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--record")) record = true;
        else path = argv[i];
    }
    std::FILE* probe = std::fopen((std::string(CORNCOB_ASSET_DIR) + "/DEF/MORNING.CCT").c_str(), "rb");
    if (!probe) {
        std::printf("test_demo: original DEF files not found, skipped\n");
        return 0;
    }
    std::fclose(probe);

    std::ifstream in(path);
    if (!in) {
        std::printf("test_demo: cannot open %s\n", path.c_str());
        return 1;
    }
    std::string theater = "MORNING", line;
    double fps = 60;
    std::vector<TimedKey> keys;
    std::vector<Expect> expect;
    std::vector<std::string> header;   // the lines kept when re-recording
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string w;
        ls >> w;
        if (w == "expect") continue;   // regenerated below
        header.push_back(line);
        if (w == "theater") ls >> theater;
        else if (w == "fps") ls >> fps;
        else if (w == "key") {
            std::string dir, hex;
            double t;
            ls >> t >> dir >> hex;
            keys.push_back({t, dir == "down", static_cast<uint8_t>(std::strtoul(hex.c_str(), nullptr, 16))});
        }
    }
    in.clear();
    in.seekg(0);
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string w;
        ls >> w;
        if (w != "expect") continue;
        Expect e{};
        ls >> e.t;
        for (int& v : e.v) ls >> v;
        expect.push_back(e);
    }
    if (expect.empty() && !record) {
        std::printf("test_demo: %s has no expect lines (run with --record)\n", path.c_str());
        return 1;
    }

    // checkpoint times: the recorded ones, or every 5 s up to the last key + 10 s when recording
    std::vector<double> times;
    if (record) {
        double last = 0;
        for (const TimedKey& k : keys) last = std::max(last, k.t);
        for (double t = 5; t <= last + 10; t += 5) times.push_back(t);
    } else {
        for (const Expect& e : expect) times.push_back(e.t);
    }

    Run r(theater.c_str(), false, fps);
    if (const char* tr = std::getenv("CC_TRACE")) {   // per frame: time oz ground topv pitch
        const double secs = std::atof(tr);
        for (int f = 0; f < secs * fps; f++) {
            std::vector<TimedKey> part;
            for (const TimedKey& k : keys) if (k.t >= f / fps - 1e-9 && k.t < (f + 1) / fps - 1e-9) part.push_back({0, k.down, k.code});
            r.flyTimed(1.0 / fps, part);
            std::printf("%.3f %d %d %d %d\n", (f + 1) / fps, r.gs.dw(d::m3::ox + 8), r.gs.b(d::m3::groundflag), r.gs.w(d::m3::topv), r.gs.w(d::m3::orvec + 2));
        }
        return 0;
    }
    std::vector<Expect> got;
    double done = 0;
    for (const double t : times) {
        std::vector<TimedKey> part;
        for (const TimedKey& k : keys)
            if (k.t >= done - 1e-9 && k.t < t - 1e-9) part.push_back({k.t - done, k.down, k.code});
        r.flyTimed(t - done, part);
        done = t;
        Expect e{};
        e.t = t;
        e.v[0] = r.gs.dw(d::m3::ox + 8);
        e.v[1] = r.gs.dw(d::m3::ox);
        e.v[2] = r.gs.dw(d::m3::ox + 4);
        e.v[3] = r.gs.w(d::m3::topv);
        e.v[4] = r.gs.w(d::m3::orvec);
        e.v[5] = r.gs.w(d::m3::orvec + 2);
        e.v[6] = r.gs.w(d::m3::orvec + 4);
        got.push_back(e);
    }

    if (record) {
        std::ofstream out(path, std::ios::trunc);
        for (const std::string& h : header) out << h << "\n";
        out << "# expect SECONDS " << NAMES << "\n";
        for (const Expect& e : got) {
            out << "expect " << e.t;
            for (const int32_t v : e.v) out << ' ' << v;
            out << "\n";
        }
        std::printf("test_demo: recorded %zu checkpoints to %s\n", got.size(), path.c_str());
        return 0;
    }

    int failures = 0;
    for (size_t i = 0; i < expect.size(); i++) {
        if (std::memcmp(expect[i].v, got[i].v, sizeof got[i].v) == 0) continue;
        failures++;
        std::printf("test_demo: mismatch at t=%.1f (%s)\n  expected:", expect[i].t, NAMES);
        for (const int32_t v : expect[i].v) std::printf(" %d", v);
        std::printf("\n  got:     ");
        for (const int32_t v : got[i].v) std::printf(" %d", v);
        std::printf("\n");
    }
    if (failures == 0) std::printf("test_demo: all %zu checkpoints match\n", expect.size());
    return failures ? 1 : 0;
}
