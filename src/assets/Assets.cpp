// Assets.cpp - Loaders for the original Corncob 3-D data files

#include "Assets.hpp"
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>

namespace corncob {

static bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

static bool fileExists(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return static_cast<bool>(f);
}

static std::string normalize(std::string p) {
    for (char& c : p) if (c == '\\') c = '/';
    return p;
}

std::string findAssetDir() {
    static std::string cached;
    if (!cached.empty()) return cached;

    std::vector<std::string> bases;
    if (char* bp = SDL_GetBasePath()) {
        bases.push_back(normalize(bp));
        SDL_free(bp);
    }
    bases.push_back("./");

    for (const std::string& base : bases) {
        std::string dir = base;
        for (int up = 0; up < 5; up++) {
            std::string cand = dir + "assets/original/";
            if (fileExists(cand + "IMG/3D.IMG")) return cached = cand;
            dir += "../";
        }
    }
    return cached = "assets/original/";
}

std::string assetPath(const std::string& relative) {
    return findAssetDir() + normalize(relative);
}

static void palFrom6Bit(const uint8_t* src, Palette& out) {
    for (int i = 0; i < 256; i++) {
        out.c[i] = {static_cast<uint8_t>((src[i * 3] & 63) * 255 / 63),
                    static_cast<uint8_t>((src[i * 3 + 1] & 63) * 255 / 63),
                    static_cast<uint8_t>((src[i * 3 + 2] & 63) * 255 / 63)};
    }
}

bool loadPalette(const std::string& path, Palette& out) {
    std::vector<uint8_t> b;
    if (!readFile(path, b) || b.size() < 768) return false;
    palFrom6Bit(b.data(), out);
    return true;
}

bool loadVga(const std::string& path, VgaImage& out) {
    std::vector<uint8_t> b;
    if (!readFile(path, b) || b.size() < 4) return false;
    int w = (b[0] | (b[1] << 8)) + 1;
    int h = (b[2] | (b[3] << 8)) + 1;
    // Same sanity limits as disppic: must fit a 320x200 mode 13h screen
    if (w <= 0 || h <= 0 || w > 320 || h > 200) return false;
    size_t n = static_cast<size_t>(w) * h;
    if (b.size() < 4 + n) return false;
    out.w = w;
    out.h = h;
    out.px.assign(b.begin() + 4, b.begin() + 4 + n);
    size_t palOff = 4 + n + static_cast<size_t>(((w + 7) & ~7) - w) * h + 2;
    out.hasPalette = b.size() >= palOff + 768;
    if (out.hasPalette) palFrom6Bit(&b[palOff], out.pal);
    return true;
}

bool loadImg(const std::string& path, EgaImg& out) {
    std::vector<uint8_t> b;
    if (!readFile(path, b) || b.size() < 4) return false;
    const size_t total = EgaImg::BPL * EgaImg::H; // dimx = 28000
    out.bits.assign(total, 0);
    size_t si = 4, di = 0;
    auto u16 = [&](size_t i) { return static_cast<size_t>(b[i] | (b[i + 1] << 8)) & 0x3FFF; };
    while (di < total && si + 3 <= b.size()) {
        size_t fill = std::min(u16(si), total - di);
        uint8_t v = b[si + 2];
        si += 3;
        for (size_t k = 0; k < fill; k++) out.bits[di++] = v;
        if (si + 2 > b.size()) break;
        size_t lit = std::min(u16(si), total - di);
        si += 2;
        for (size_t k = 0; k < lit && si < b.size(); k++) out.bits[di++] = b[si++];
    }
    return true;
}

// Reads script lines, stripping CR and trailing whitespace
static std::vector<std::string> readLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream f(path, std::ios::binary);
    std::string s;
    while (std::getline(f, s)) {
        while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
        lines.push_back(s);
    }
    return lines;
}

static bool toInt(const std::string& s, int& v) {
    const char* p = s.c_str();
    while (*p == ' ' || *p == '\t') p++;
    char* end = nullptr;
    long r = std::strtol(p, &end, 10);
    if (end == p) return false;
    v = static_cast<int>(r);
    return true;
}

std::vector<PictPage> loadTitleScript(const std::string& path) {
    std::vector<PictPage> pages;
    std::vector<std::string> L = readLines(path);
    size_t i = 0;
    auto next = [&](std::string& s) { if (i >= L.size()) return false; s = L[i++]; return true; };
    std::string s;
    while (next(s)) {
        if (s.empty()) continue;
        PictPage pg;
        int nlines = 0;
        if (!toInt(s, pg.x) || pg.x < 0) break; // -1 terminates
        std::string t;
        if (!next(t) || !toInt(t, pg.y)) break;
        if (!next(pg.file)) break;
        if (!next(t) || !toInt(t, nlines)) break;
        if (!next(t) || !toInt(t, pg.color)) break;
        for (int k = 0; k < nlines; k++) {
            PictLine ln{};
            if (!next(t) || !toInt(t, ln.col)) break;
            if (!next(t) || !toInt(t, ln.row)) break;
            if (!next(ln.text)) break;
            pg.lines.push_back(ln);
        }
        pages.push_back(pg);
    }
    return pages;
}

std::vector<PictPage> loadMissionScript(const std::string& path) {
    std::vector<PictPage> pages;
    std::vector<std::string> L = readLines(path);
    size_t i = 0;
    auto next = [&](std::string& s) { if (i >= L.size()) return false; s = L[i++]; return true; };
    std::string s;
    while (i < L.size()) {
        // Skip separators (^L) and blank lines between entries
        while (i < L.size() && (L[i].empty() || L[i][0] == '\f')) i++;
        if (i >= L.size()) break;
        PictPage pg;
        int nlines = 0;
        if (!next(s) || !toInt(s, nlines)) break;
        if (!next(s) || !toInt(s, pg.x)) break;
        if (!next(s) || !toInt(s, pg.y)) break;
        if (!next(pg.file)) break;
        if (!next(s) || !toInt(s, pg.color)) break;
        for (int k = 0; k < nlines; k++) {
            PictLine ln{};
            if (!next(s) || !toInt(s, ln.row)) break;
            if (!next(s) || !toInt(s, ln.col)) break;
            if (!next(ln.text)) break;
            pg.lines.push_back(ln);
        }
        pages.push_back(pg);
        // Skip to the next ^L
        while (i < L.size() && !(L[i].size() && L[i][0] == '\f')) i++;
    }
    return pages;
}

} // namespace corncob
