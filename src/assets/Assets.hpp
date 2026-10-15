// Assets.hpp - Loaders for the original Corncob 3-D data files
//
// .VGA  (PICT.ASM disppic): u16 width-1, u16 height-1, width*height 8-bit pixels,
//       then (roundup8(width)-width)*height pad bytes + 2 junk bytes, then an
//       optional 256-entry 6-bit RGB palette. Logos ship without a palette.
// .PAL  : 256 x 3 bytes, 6-bit VGA DAC values.
// .IMG  (RIMG.ASM): 4-byte header, then repeated [u16 fill count][u8 fill byte]
//       [u16 literal count][literal bytes]; counts masked to 14 bits. Decodes to a
//       640x350 1-bit EGA bitmap (80 bytes/row) drawn in color 7 over black.
// PICT.DAT / TPICT.DAT: line-oriented picture+caption scripts (see PictPage).

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace corncob {

struct Rgb { uint8_t r, g, b; };

struct Palette {
    Rgb c[256]{};
};

struct VgaImage {
    int w = 0, h = 0;
    std::vector<uint8_t> px;
    bool hasPalette = false;
    Palette pal;
};

struct EgaImg {
    static constexpr int W = 640, H = 350, BPL = 80;
    std::vector<uint8_t> bits; // W/8 * H bytes, MSB = leftmost pixel
    bool pixel(int x, int y) const { return bits[y * BPL + (x >> 3)] & (0x80 >> (x & 7)); }
};

struct PictLine {
    int col, row;      // text cell position (8x8 cells on the 320x200 screen)
    std::string text;
};

struct PictPage {
    int x = 0, y = 0;  // picture position in pixels
    std::string file;  // relative path as written in the script (e.g. "vga\car5cj.vga")
    int color = 15;    // caption color index (in the picture's palette)
    std::vector<PictLine> lines;
};

// Locates the extracted original game directory (contains IMG\3D.IMG).
// Searches <exe dir>/assets/original and its parents, then the working directory.
std::string findAssetDir();
std::string assetPath(const std::string& relative); // joins and normalizes '\' -> '/'

bool loadPalette(const std::string& path, Palette& out);
bool loadVga(const std::string& path, VgaImage& out);
bool loadImg(const std::string& path, EgaImg& out);

// TPICT.DAT (title story, read by CCTITLE.EXE): x, y, file, nlines, color,
// then nlines x (col, row, text); list ends with -1.
std::vector<PictPage> loadTitleScript(const std::string& path);

// PICT.DAT (mission-end pictures, PICT.ASM readpict): nlines, x, y, file, color,
// then nlines x (row, col, text); entries separated by a ^L line.
std::vector<PictPage> loadMissionScript(const std::string& path);

} // namespace corncob
