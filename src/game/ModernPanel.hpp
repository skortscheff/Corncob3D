// ModernPanel.hpp - A smooth, anti-aliased instrument panel drawn over the window instead of the EGA one
//
// The game keeps drawing the original 640x350 panel into its EGA pages (nothing in the simulation changes);
// this class reads the same state the original needles read (DGROUP) and paints new gauges over the lower
// part of the displayed picture at window resolution. The rear-view window and the radar stay the EGA
// picture: they are holes in the overlay. The lamps that flash (stall, eject, low altitude ...) and the
// damage bars are read back from the EGA panel so that they behave exactly as the original ones.
//
// Layout constants are the 640x350 coordinates of the original panel (3D.IMG, INST.ASM records).

#pragma once
#include "game/GameState.hpp"
#include "render/EgaScreen.hpp"
#include <SDL.h>
#include <cstdint>
#include <vector>

namespace corncob::game {

class ModernPanel {
public:
    explicit ModernPanel(const GameState& gs) : m_g(gs) {}
    ~ModernPanel();
    ModernPanel(const ModernPanel&) = delete;
    ModernPanel& operator=(const ModernPanel&) = delete;

    // True while the cockpit panel is what the game is showing (not the map, the help pictures ...)
    bool active() const;

    // Paints the panel over the picture just copied to the renderer; `dst` is where that picture went
    void draw(SDL_Renderer* sdl, const SDL_Rect& dst, const EgaScreen& scr, const EgaPalette& pal);

    // The EGA picture with the panel on it, w x h pixels (a 4:3 picture), as a BMP
    bool saveComposite(const EgaScreen& scr, const EgaPalette& pal, const char* path, int w = 1280, int h = 960);

private:
    struct Canvas {
        int w = 0, h = 0;
        std::vector<uint32_t> px;   // 0xAARRGGBB, straight alpha (only the holes are transparent)
    };

    void ensure(int w, int h);
    void buildStatic();
    void paint(const EgaScreen& scr, const EgaPalette& pal);   // static layer + the live parts, into m_frame

    const GameState& m_g;
    SDL_Texture* m_tex = nullptr;
    SDL_Renderer* m_sdl = nullptr;
    int m_w = 0, m_h = 0;   // size of the 4:3 picture this was built for
    int m_top = 0;          // first row of the band, in picture pixels
    float m_sx = 1, m_sy = 1;
    Canvas m_static, m_frame;
    std::vector<SDL_Rect> m_holes;   // in canvas pixels
};

} // namespace corncob::game
