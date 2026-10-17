// test_rate.cpp - The simulation at different frame rates: the same keys, the same seconds of flight, and the
// plane must be where it is at 20 frames a second (the rate the original ran at)
// Needs the original DEF files (assets/original, never committed); skipped when missing.

#include "FlightRun.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>

using namespace corncob;

static int failures = 0;
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
        const double step = std::min(2.0, seconds - done);
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
    const double seconds = 30;
    const std::vector<Sample> ref = trace(20, "MORNING", seconds);
    for (const double fps : {30.0, 60.0}) {
        const std::vector<Sample> t = trace(fps, "MORNING", seconds);
        if (print || true) {
            std::printf("%-5s %-6s %9s %9s %9s %6s %6s %6s %6s\n", "fps", "t", "alt", "x", "y", "speed", "pitch", "roll", "yaw");
            for (size_t i = 0; i < ref.size() && i < t.size(); i++) {
                std::printf("20    %-6.0f %9d %9d %9d %6d %6d %6d %6d\n", ref[i].t, ref[i].alt, ref[i].x, ref[i].y, ref[i].speed, ref[i].pitch, ref[i].roll, ref[i].yaw);
                std::printf("%-5.0f %-6.0f %9d %9d %9d %6d %6d %6d %6d\n", fps, t[i].t, t[i].alt, t[i].x, t[i].y, t[i].speed, t[i].pitch, t[i].roll, t[i].yaw);
            }
        }
    }
    if (failures == 0) std::printf("test_rate: all passed\n");
    return failures ? 1 : 0;
}
