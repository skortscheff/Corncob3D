// Renderer.hpp - Wireframe 3D renderer onto a 640x350 EGA-style framebuffer
// Replaces f3dvec.asm (f3dpt projection, clip.asm line clipping) and the
// EGA/HGC line drawers (EGALINE/HLINE/DLINE...) with SDL_Renderer lines.
// Window geometry and projection planes come from 3DVID.INC (vgmode 10h).

#pragma once
#include "../assets/Assets.hpp"
#include "../math/Mat3x3.hpp"
#include <SDL.h>
#include <cstdint>
#include <utility>
#include <vector>

namespace corncob {

// Standard 16-color EGA palette (COLORS.ASM indexes into this)
enum EgaColor : uint8_t {
    BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LIGHTGRAY,
    DARKGRAY, LIGHTBLUE, LIGHTGREEN, LIGHTCYAN, LIGHTRED, LIGHTMAGENTA, YELLOW, WHITE
};

struct P3 { float x, y, z; };

// Polygon (type 8 in the ASM object lists) or open polyline (a wire in a type-2 frame)
struct Poly {
    uint8_t color;
    std::vector<P3> pts;
    bool closed = true;
};

// Sphere (type 6): drawn as a screen-space circle
struct Sphere {
    P3 c;
    float r;
    uint8_t color;
};

// Object model: edge-list wireframe and/or decoded polygons and spheres
struct Model {
    std::vector<P3> verts;
    std::vector<std::pair<int, int>> edges;
    uint8_t color = WHITE;
    std::vector<Poly> polys;
    std::vector<Sphere> spheres;
};

class Renderer {
public:
    static constexpr int WIDTH = 640;   // vxsiz
    static constexpr int HEIGHT = 350;  // vysiz

    // Main 3D window (gwinf in 3DVID.INC)
    static constexpr int GFX_MIN_X = 8, GFX_MIN_Y = 4, GFX_MAX_X = 631, GFX_MAX_Y = 199;
    static constexpr float GFX_PLN_X = 300.0f, GFX_PLN_Y = 200.0f; // projection plane distances
    static constexpr float GFX_CTR_X = (GFX_MIN_X + GFX_MAX_X) / 2, GFX_CTR_Y = (GFX_MIN_Y + GFX_MAX_Y) / 2;
    // Secondary window in the middle of the instrument panel (gwint)
    static constexpr int GT_MIN_X = 168, GT_MIN_Y = 254, GT_MAX_X = 471, GT_MAX_Y = 344;

    explicit Renderer(SDL_Renderer* sdl);
    ~Renderer();

    // Clears and restricts drawing to the 3D window
    void beginFrame(uint8_t clearColor = BLACK);
    void endFrame();
    bool saveScreenshot(const char* path); // call before endFrame

    // Instrument panel (IMG\3D.IMG); rows below the 3D window are drawn by drawPanel
    void setPanel(const EgaImg& img);
    // Lifts the 3D-window clip and overlays the panel; HUD drawing after this is unclipped
    void drawPanel();

    // Camera: position + orientation (rows = right, up, forward in world space)
    void setCamera(const P3& pos, const Mat3x3& orient);

    void line3D(const P3& a, const P3& b, uint8_t color);
    void model(const Model& m, const P3& pos, float yawDeg = 0.0f);
    // Full orientation (rows = body right, up, forward in world space) and uniform scale
    void model(const Model& m, const P3& pos, const Mat3x3& orient, float scale);
    void sphere3D(const P3& c, float r, uint8_t color);

    // Sky/ground fill of the 3D window, accounting for pitch and roll
    void horizon(uint8_t sky, uint8_t ground);

    // Screen-space drawing for the HUD
    void line2D(int x0, int y0, int x1, int y1, uint8_t color);
    void rect2D(int x, int y, int w, int h, uint8_t color, bool filled);
    // 8x8 BIOS-style text at pixel position
    void text(int x, int y, const char* s, uint8_t color, int scale = 1);

private:
    void setColor(uint8_t c);
    bool toView(const P3& w, P3& v) const;
    P3 project(const P3& v) const;

    SDL_Renderer* m_sdl;
    SDL_Texture* m_target = nullptr;
    SDL_Texture* m_panel = nullptr;
    P3 m_camPos{0, 0, 0};
    Mat3x3 m_camOrient;
};

} // namespace corncob
