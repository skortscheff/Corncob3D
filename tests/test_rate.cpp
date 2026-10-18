// test_rate.cpp - The simulation at different frame rates: the same keys, the same seconds of flight, and the
// plane must be where it is at 20 frames a second (the rate the original ran at)
// Needs the original DEF files (assets/original, never committed); skipped when missing.

#include "FlightRun.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>

using namespace corncob;

static int failures = 0;
static double g_step = 2;   // seconds between samples
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                      \
        }                                                                    \
    } while (0)

struct Sample {
    double t;
    int32_t alt, x, y;
    int16_t speed, yaw, pitch, roll;
    int16_t rpm;
    bool ground;
    int32_t pitdot;
    int16_t pitchmin;
};

// takeoff (full throttle, back stick at speed), a climb, a banked turn, then the stick centered again
static std::vector<Sample> trace(double fps, const char* theater, double seconds) {
    Run r(theater, false, fps);
    std::vector<Sample> out;
    const std::vector<TimedKey> keys = {{0.2, true, 0x4E},  {1.0, false, 0x4E},   // the throttle up
                                        {13.5, true, 0x50}, {14.3, false, 0x50},  // the stick back: off the ground
                                        {20.0, true, 0x4D}, {21.0, false, 0x4D},  // roll right for a second
                                        {21.0, true, 0x50}, {22.0, false, 0x50}}; // and pull
    double done = 0;
    while (done < seconds && !r.game.done()) {
        const double step = std::min(g_step, seconds - done);
        // the keys that fall inside this step, shifted to its start
        std::vector<TimedKey> part;
        for (const TimedKey& k : keys)
            if (k.t >= done - 1e-9 && k.t < done + step - 1e-9) part.push_back({k.t - done, k.down, k.code});
        r.flyTimed(step, part);
        done += step;
        Sample s;
        s.t = done;
        s.alt = r.gs.dw(d::m3::ox + 8);
        s.x = r.gs.dw(d::m3::ox);
        s.y = r.gs.dw(d::m3::ox + 4);
        s.speed = r.gs.w(d::m3::topv);
        s.yaw = r.gs.w(d::m3::orvec);
        s.pitch = r.gs.w(d::m3::orvec + 2);
        s.roll = r.gs.w(d::m3::orvec + 4);
        s.rpm = r.gs.w(d::m3::rpm);
        s.ground = r.gs.b(d::m3::groundflag) != 0;
        s.pitdot = r.gs.dw(d::m3::pitdotl);
        s.pitchmin = r.gs.w(d::m3::pitchmin);
        out.push_back(s);
    }
    return out;
}

int main(int argc, char** argv) {
    std::FILE* probe = std::fopen((std::string(CORNCOB_ASSET_DIR) + "/DEF/MORNING.CCT").c_str(), "rb");
    if (!probe) {
        std::printf("test_rate: original DEF files not found, skipped\n");
        return 0;
    }
    std::fclose(probe);
    const bool print = argc > 1 && !std::strcmp(argv[1], "--print");
    const double seconds = argc > 3 ? std::atof(argv[3]) : 30;
    if (argc > 2) g_step = std::atof(argv[2]);
    const std::vector<Sample> ref = trace(20, "MORNING", seconds);
    std::vector<double> rates = {60.0};   // (30 climbs steeper than 20 and 60: not offered)
    if (const char* e = std::getenv("CC_RATES")) { rates.clear(); for (const char* p = e; *p;) { rates.push_back(std::atof(p)); while (*p && *p != ',') p++; if (*p) p++; } }
    for (const double fps : rates) {
        const std::vector<Sample> t = trace(fps, "MORNING", seconds);
        if (print) {
            std::printf("%-5s %-6s %9s %9s %9s %6s %6s %6s %6s\n", "fps", "t", "alt", "x", "y", "speed", "pitch", "roll", "yaw");
            for (size_t i = 0; i < ref.size() && i < t.size(); i++) {
                std::printf("20    %-6.1f %9d %9d %9d %6d %6d %6d %6d %9d %6d\n", ref[i].t, ref[i].alt, ref[i].x, ref[i].y, ref[i].speed, ref[i].pitch, ref[i].roll, ref[i].yaw, ref[i].pitdot, ref[i].pitchmin);
                std::printf("%-5.0f %-6.1f %9d %9d %9d %6d %6d %6d %6d %9d %6d\n", fps, t[i].t, t[i].alt, t[i].x, t[i].y, t[i].speed, t[i].pitch, t[i].roll, t[i].yaw, t[i].pitdot, t[i].pitchmin);
            }
        }
        // the plane of the other rates must be where the 20 fps plane is (the samples are 2 s apart)
        for (size_t i = 0; i < ref.size() && i < t.size(); i++) {
            const double sec = ref[i].t;
            if (sec != 10 && sec != 14 && sec != 20) continue;
            const double tol = 0.07;
            CHECK(std::abs(t[i].speed - ref[i].speed) <= tol * ref[i].speed);
            CHECK(std::abs(t[i].x - ref[i].x) <= 5000);                       // 3% of the distance flown
            if (sec == 20) {
                CHECK(std::abs(t[i].alt - ref[i].alt) <= 0.15 * ref[i].alt);  // the climb
                CHECK(std::abs(t[i].pitch - ref[i].pitch) <= 400);
            }
            if (sec == 10) CHECK(t[i].ground && ref[i].ground);
        }
    }
    if (failures == 0) std::printf("test_rate: all passed\n");
    return failures ? 1 : 0;
}
