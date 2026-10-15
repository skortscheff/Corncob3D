// main.cpp - Corncob 3-D (Windows/SDL2 port) entry point and main loop
// Stands in for the main loop in 3.ASM: poll input, update the plane, draw the frame, flip.
//
// Status: flight + world rendering scaffold. The original object models (DEFOBJS.ASM),
// enemies, weapons, and mission logic are not ported yet; the scene uses placeholder models.

#include "math/Mat3x3.hpp"
#include "math/Trig.hpp"
#include "platform/Input.hpp"
#include "platform/Window.hpp"
#include "render/Renderer.hpp"
#include "game/PlayerObjects.hpp"
#include "assets/Assets.hpp"
#include "ui/Intro.hpp"
#include <string>

#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace corncob;

namespace {

Model makeBox(float w, float h, float d, uint8_t color) {
    Model m;
    float x = w / 2, z = d / 2;
    m.verts = {{-x, 0, -z}, {x, 0, -z}, {x, 0, z}, {-x, 0, z},
               {-x, h, -z}, {x, h, -z}, {x, h, z}, {-x, h, z}};
    m.edges = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    m.color = color;
    return m;
}

Model makePyramid(float w, float h, uint8_t color) {
    Model m;
    float x = w / 2;
    m.verts = {{-x, 0, -x}, {x, 0, -x}, {x, 0, x}, {-x, 0, x}, {0, h, 0}};
    m.edges = {{0,1},{1,2},{2,3},{3,0},{0,4},{1,4},{2,4},{3,4}};
    m.color = color;
    return m;
}

// Control tower: tall shaft with a wider cab on top (TOWER in DEFOBJS.ASM, approximated)
Model makeTower(uint8_t color) {
    Model m = makeBox(20, 120, 20, color);
    Model cab = makeBox(50, 25, 50, color);
    int base = static_cast<int>(m.verts.size());
    for (auto v : cab.verts) m.verts.push_back({v.x, v.y + 120, v.z});
    for (auto e : cab.edges) m.edges.push_back({e.first + base, e.second + base});
    return m;
}

struct Placed { const Model* model; P3 pos; float yaw; };

struct Plane {
    P3 pos{0, 6, -400};
    Mat3x3 orient;     // rows: right, up, forward
    float speed = 0;   // units/sec
    float throttle = 0; // 0..1
    bool crashed = false;
};

constexpr float MAX_SPEED = 600.0f;
constexpr float GROUND_Y = 0.0f;
constexpr float EYE_HEIGHT = 6.0f;

void updatePlane(Plane& p, const Input& in, float dt) {
    if (in.isKeyJustDown(SDL_SCANCODE_R)) p = Plane{};
    if (p.crashed) return;

    // Throttle: A/Z like a keyboard throttle, or number keys 0-9 for preset settings
    if (in.isKeyDown(SDL_SCANCODE_A)) p.throttle = std::min(1.0f, p.throttle + 0.5f * dt);
    if (in.isKeyDown(SDL_SCANCODE_Z)) p.throttle = std::max(0.0f, p.throttle - 0.5f * dt);
    for (int k = 0; k <= 9; k++) {
        SDL_Scancode sc = k == 0 ? SDL_SCANCODE_0 : static_cast<SDL_Scancode>(SDL_SCANCODE_1 + k - 1);
        if (in.isKeyJustDown(sc)) p.throttle = k == 0 ? 1.0f : k / 10.0f;
    }

    float pitch = 0, roll = 0, yaw = 0;
    if (in.isKeyDown(SDL_SCANCODE_UP)) pitch -= 1;     // stick forward = nose down
    if (in.isKeyDown(SDL_SCANCODE_DOWN)) pitch += 1;
    if (in.isKeyDown(SDL_SCANCODE_LEFT)) roll -= 1;
    if (in.isKeyDown(SDL_SCANCODE_RIGHT)) roll += 1;
    if (in.isKeyDown(SDL_SCANCODE_COMMA)) yaw -= 1;
    if (in.isKeyDown(SDL_SCANCODE_PERIOD)) yaw += 1;
    if (in.hasJoystick()) {
        roll += in.joystickAxis(0);
        pitch += in.joystickAxis(1);
    }

    // Speed chases throttle; no lift model yet, so the plane flies where it points
    float target = p.throttle * MAX_SPEED;
    p.speed += (target - p.speed) * std::min(1.0f, dt * 0.8f);

    // Control authority scales with airspeed so you can't spin on the runway
    float authority = std::min(1.0f, p.speed / 120.0f);
    bool onGround = p.pos.y <= GROUND_Y + EYE_HEIGHT + 0.01f;

    // Banking turns the plane (coordinated turn approximation)
    const float* r = &p.orient.m[0];
    float bank = -r[1]; // >0 when banked right
    float bankYaw = bank * 60.0f * authority;

    p.orient.rotate((yaw * 40.0f * authority + bankYaw) * dt,
                    pitch * 60.0f * authority * dt,
                    roll * 90.0f * authority * dt);
    p.orient.orthonormalize();

    const float* f = &p.orient.m[6];
    p.pos.x += f[0] * p.speed * dt;
    p.pos.y += f[1] * p.speed * dt;
    p.pos.z += f[2] * p.speed * dt;

    // Below takeoff speed the plane can't climb
    if (p.speed < 100.0f && p.pos.y > GROUND_Y + EYE_HEIGHT && onGround)
        p.pos.y = GROUND_Y + EYE_HEIGHT;

    if (p.pos.y < GROUND_Y + EYE_HEIGHT) {
        // Gentle, level contact = landing; anything else = crash (crshflg in 3.ASM)
        float sink = -f[1] * p.speed;
        float levelness = p.orient.m[4]; // up.y
        if (sink > 40.0f || levelness < 0.9f) {
            p.crashed = true;
            p.speed = 0;
        }
        p.pos.y = GROUND_Y + EYE_HEIGHT;
        if (!p.crashed && f[1] < 0) {
            // Settle nose onto the ground
            p.orient.rotate(0, -std::asin(f[1]) * RAD2DEG, 0);
            p.orient.orthonormalize();
        }
    }
}

void drawGround(Renderer& r, const P3& cam) {
    // Grid of ground lines around the camera, snapped so it doesn't swim
    const float cell = 250.0f;
    const int n = 16;
    float ox = std::floor(cam.x / cell) * cell;
    float oz = std::floor(cam.z / cell) * cell;
    for (int i = -n; i <= n; i++) {
        r.line3D({ox + i * cell, 0, oz - n * cell}, {ox + i * cell, 0, oz + n * cell}, GREEN);
        r.line3D({ox - n * cell, 0, oz + i * cell}, {ox + n * cell, 0, oz + i * cell}, GREEN);
    }
}

void drawRunway(Renderer& r) {
    const float hw = 30, len = 1500;
    r.line3D({-hw, 0.1f, -500}, {-hw, 0.1f, -500 + len}, LIGHTGRAY);
    r.line3D({hw, 0.1f, -500}, {hw, 0.1f, -500 + len}, LIGHTGRAY);
    r.line3D({-hw, 0.1f, -500}, {hw, 0.1f, -500}, LIGHTGRAY);
    r.line3D({-hw, 0.1f, -500 + len}, {hw, 0.1f, -500 + len}, LIGHTGRAY);
    for (float z = -480; z < -500 + len; z += 80)
        r.line3D({0, 0.1f, z}, {0, 0.1f, z + 40}, WHITE);
}

// Call after Renderer::drawPanel
void drawHud(Renderer& r, const Plane& p, float fps) {
    const int cx = static_cast<int>(Renderer::GFX_CTR_X), cy = static_cast<int>(Renderer::GFX_CTR_Y);
    r.line2D(cx - 10, cy, cx - 3, cy, YELLOW);
    r.line2D(cx + 3, cy, cx + 10, cy, YELLOW);
    r.line2D(cx, cy - 6, cx, cy - 2, YELLOW);

    float yaw, pitch, roll;
    p.orient.euler(yaw, pitch, roll);
    int hdg = static_cast<int>(std::lround(yaw + 360.0f)) % 360;

    // Temporary digital readouts in the panel's center window until the
    // needle gauges (drawndles in INST.ASM) are ported
    char buf[64];
    const int tx = Renderer::GT_MIN_X + 12, ty = Renderer::GT_MIN_Y + 10;
    std::snprintf(buf, sizeof buf, "AIRSPEED %4d MPH", static_cast<int>(p.speed / 2));
    r.text(tx, ty, buf, LIGHTGREEN);
    std::snprintf(buf, sizeof buf, "ALTITUDE %4d", static_cast<int>(p.pos.y - EYE_HEIGHT));
    r.text(tx, ty + 14, buf, LIGHTGREEN);
    std::snprintf(buf, sizeof buf, "HEADING   %03d", hdg);
    r.text(tx, ty + 28, buf, LIGHTGREEN);
    std::snprintf(buf, sizeof buf, "THROTTLE  %3d%%", static_cast<int>(p.throttle * 100));
    r.text(tx, ty + 42, buf, LIGHTGREEN);
    std::snprintf(buf, sizeof buf, "%3d FPS", static_cast<int>(fps));
    r.text(Renderer::GT_MAX_X - 64, Renderer::GT_MAX_Y - 12, buf, DARKGRAY);

    if (p.crashed) {
        r.text(cx - 56, cy - 30, "CRASHED", LIGHTRED, 2);
        r.text(cx - 72, cy + 10, "PRESS R TO RESTART", WHITE);
    } else if (p.speed < 1.0f) {
        r.text(cx - 212, 12, "A/Z THROTTLE  ARROWS FLY  , . RUDDER  V VIEW  ESC QUIT", WHITE);
    }
}

} // namespace

int main(int argc, char* argv[]) {
    // --screenshot <file.bmp>: skip the intro, render a few frames in flight, save one, exit
    // --shot-intro <prefix>:   run intro + loading screen unattended, save one BMP per scene, exit
    // --skip-intro:            go straight to the loading screen and game
    const char* shotPath = nullptr;
    const char* introShots = nullptr;
    bool skipIntro = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) shotPath = argv[++i];
        else if (std::strcmp(argv[i], "--shot-intro") == 0 && i + 1 < argc) introShots = argv[++i];
        else if (std::strcmp(argv[i], "--skip-intro") == 0) skipIntro = true;
    }
    int frame = 0;

    init_sin_table();

    Window window;
    if (!window.init()) return 1;

    if (!shotPath) {
        std::string prefix = introShots ? introShots : "";
        if (!skipIntro && runIntro(window, introShots ? prefix.c_str() : nullptr) == IntroResult::Quit)
            return 0;
        std::string loadPrefix = prefix + "load";
        if (runLoadingScreen(window, introShots ? loadPrefix.c_str() : nullptr) == IntroResult::Quit)
            return 0;
        if (introShots) return 0;
    }

    Renderer renderer(window.sdlRenderer());
    EgaImg panel;
    if (loadImg(assetPath("IMG/3D.IMG"), panel)) renderer.setPanel(panel);
    Input input;
    input.initJoystick();

    // Scene: placeholder stand-ins for the airfield, HQs and targets
    Model tower = makeTower(LIGHTGRAY);
    Model hangar = makeBox(120, 40, 80, BROWN);
    Model hq = makeBox(80, 60, 80, LIGHTRED);
    Model pyramid = makePyramid(150, 120, YELLOW);
    Model bunker = makeBox(40, 15, 40, LIGHTMAGENTA);

    std::vector<Placed> scene = {
        {&tower, {90, 0, -300}, 0},
        {&hangar, {140, 0, -150}, 0},
        {&hangar, {140, 0, 0}, 0},
        {&hq, {2500, 0, 3000}, 20},
        {&hq, {-3000, 0, 4500}, -35},
        {&pyramid, {-1200, 0, 2200}, 10},
        {&pyramid, {1800, 0, 6000}, 45},
    };
    for (int i = 0; i < 12; i++) {
        float a = i * 30.0f * DEG2RAD;
        scene.push_back({&bunker, {std::cos(a) * 5000, 0, 3000 + std::sin(a) * 3500}, i * 17.0f});
    }

    Plane plane;
    if (shotPath) {
        // Airborne over the field, banked slightly, looking toward the targets
        plane.pos = {-150, 180, -700};
        plane.orient.rotate(15, -8, 12);
        plane.speed = 300;
        plane.throttle = 0.5f;
    }
    bool chaseView = shotPath != nullptr;
    float propAngle = 0.0f;
    // Original object units -> world units: the ~2500-long plane becomes ~40 long
    constexpr float PLANE_SCALE = 1.0f / 60.0f;
    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    float fps = 60.0f;

    while (window.pollEvents()) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>((now - prev) / freq);
        prev = now;
        dt = std::min(dt, 0.1f);
        if (dt > 0) fps = fps * 0.95f + (1.0f / dt) * 0.05f;

        input.update();
        if (input.isKeyJustDown(SDL_SCANCODE_ESCAPE)) break;
        updatePlane(plane, input, dt);

        renderer.beginFrame();
        if (input.isKeyJustDown(SDL_SCANCODE_V)) chaseView = !chaseView;
        propAngle += (200.0f + plane.throttle * 2000.0f) * dt;

        P3 camPos = plane.pos;
        Mat3x3 camOrient = plane.orient;
        if (chaseView) {
            // Behind and above, looking slightly down at the plane
            const float* u = &plane.orient.m[3];
            const float* f = &plane.orient.m[6];
            camPos = {plane.pos.x - f[0] * 70 + u[0] * 18,
                      plane.pos.y - f[1] * 70 + u[1] * 18,
                      plane.pos.z - f[2] * 70 + u[2] * 18};
            camPos.y = std::max(camPos.y, 2.0f);
            camOrient = Mat3x3::rotX(-12.0f) * plane.orient;
        }
        renderer.setCamera(camPos, camOrient);
        renderer.horizon(BLUE, BLACK);
        drawGround(renderer, plane.pos);
        drawRunway(renderer);
        for (auto& o : scene) renderer.model(*o.model, o.pos, o.yaw);
        if (chaseView) {
            float dx = plane.pos.x - camPos.x, dy = plane.pos.y - camPos.y, dz = plane.pos.z - camPos.z;
            float distOrig = std::sqrt(dx * dx + dy * dy + dz * dz) / PLANE_SCALE;
            bool med = distOrig > PLANE_MED_LOD_DIST;
            const Model& body = plane.crashed ? playerPlaneSmokeModel()
                              : med ? playerPlaneMedModel() : playerPlaneModel();
            renderer.model(body, plane.pos, plane.orient, PLANE_SCALE);
            if (!plane.crashed) {
                // Spin the prop about the body's forward axis
                Mat3x3 propOrient = Mat3x3::rotZ(propAngle) * plane.orient;
                renderer.model(med ? propellerMedModel() : propellerModel(), plane.pos, propOrient, PLANE_SCALE);
            }
        }
        renderer.drawPanel();
        drawHud(renderer, plane, fps);
        if (shotPath && ++frame == 10) {
            renderer.saveScreenshot(shotPath);
            window.setQuit();
        }
        renderer.endFrame();
    }
    return 0;
}
