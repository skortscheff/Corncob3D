// EgaScreen.hpp - The two EGA mode-10h display pages, as indexed pixels
//
// The original draws into the hidden page (vboff) while the other is shown, and
// flipage swaps them. Nothing clears a page between frames: the horizon fill
// repaints the 3D windows and epage the borders, so leftovers from two frames
// ago persist exactly as on the real hardware. Pixel values are EGA color
// numbers 0-15, turned into RGB through the attribute/DAC palette at present time.
//
// HD mode is the same page at an integer multiple of 640x350; the engine scales
// its window constants to match.

#pragma once
#include "../assets/Assets.hpp"
#include <SDL.h>
#include <array>
#include <cstdint>
#include <vector>

namespace corncob {

enum EgaColor : uint8_t {
    BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LIGHTGRAY,
    DARKGRAY, LIGHTBLUE, LIGHTGREEN, LIGHTCYAN, LIGHTRED, LIGHTMAGENTA, YELLOW, WHITE
};

class EgaScreen {
public:
    static constexpr int BASE_W = 640, BASE_H = 350;

    explicit EgaScreen(int scale = 1) { setScale(scale); }

    void setScale(int s);
    int scale() const { return m_scale; }
    int width() const { return m_w; }
    int height() const { return m_h; }

    // The page being drawn (vboff) and the page on display
    uint8_t* back() { return m_pages[m_back].data(); }
    const uint8_t* front() const { return m_pages[m_back ^ 1].data(); }
    void flip() { m_back ^= 1; }

    // Which of the four bit planes writes reach (the sequencer map mask): the needles of the
    // instruments are drawn into plane 3 only, so they pass over the numerals without erasing them
    void setPlaneMask(uint8_t mask) { m_mask = mask & 15; }
    uint8_t planeMask() const { return m_mask; }

    void pset(int x, int y, uint8_t c) {
        if (static_cast<unsigned>(x) < static_cast<unsigned>(m_w) && static_cast<unsigned>(y) < static_cast<unsigned>(m_h)) {
            uint8_t& p = m_pages[m_back][static_cast<size_t>(y) * m_w + x];
            p = m_mask == 15 ? c : static_cast<uint8_t>((p & ~m_mask) | (c & m_mask));
        }
    }
    // Replaces every pixel of color `from` by `to` in the rectangle (inclusive, 640x350 coordinates),
    // in both pages (the EGA read-mode-1 color compare of drawbutton)
    void replaceColor(int x0, int y0, int x1, int y1, uint8_t from, uint8_t to);
    // Fills a rectangle (already scaled coordinates) in both pages with all planes (rawhline
    // done once per page, as the damage bars and buttons are)
    void fillRectBoth(int x, int y, int w, int h, uint8_t c);
    // Pixels [x0, x1) of row y, clipped to the page
    void hspan(int x0, int x1, int y, uint8_t c);
    void fillRect(int x, int y, int w, int h, uint8_t c);

    // 1-bit panel image in color 7 over black, into both pages (scaled in HD)
    void drawPanel(const EgaImg& img, int fromRow);
    // 8x8 text in one color, into the back page; x/y in 640x350 coordinates
    void text(int x, int y, const char* s, uint8_t color);

private:
    int m_scale = 1, m_w = BASE_W, m_h = BASE_H;
    int m_back = 0;
    uint8_t m_mask = 15;
    std::array<std::vector<uint8_t>, 2> m_pages;
};

// The 16-entry palette: EGA attribute registers pointing into the VGA DAC
struct EgaPalette {
    std::array<uint8_t, 16> attr{};          // color number -> DAC index
    std::array<std::array<uint8_t, 3>, 64> dac{}; // 6-bit RGB
    static EgaPalette defaults();            // BIOS mode 10h state
    Rgb rgb(uint8_t color) const;
};

// Uploads the front page through the palette and letterboxes it to 4:3
class Display {
public:
    explicit Display(SDL_Renderer* sdl) : m_sdl(sdl) {}
    ~Display();
    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    // panX/panY: start-address offset in pixels (screen shake, see flipage)
    void present(const EgaScreen& scr, const EgaPalette& pal, int panX = 0, int panY = 0);
    bool saveBmp(const EgaScreen& scr, const EgaPalette& pal, const char* path) const;

private:
    void convert(const EgaScreen& scr, const EgaPalette& pal, int panX, int panY);

    SDL_Renderer* m_sdl;
    SDL_Texture* m_tex = nullptr;
    int m_texW = 0, m_texH = 0;
    std::vector<uint32_t> m_px;
};

} // namespace corncob
