// Main3Panel.cpp - The button and damage procs of 3.ASM that drive the instrument panel
//
// Every drawbutton call passes (new color << 8) | color to replace, and a button number (see
// Instruments::drawbutton). Colors 7 and 15 are the panel's lettering, 0 the button's background.

#include "Main3.hpp"
#include "DGroup.hpp"

namespace corncob::game {

namespace lbl = corncob::lbl;

#define M3(name) lbl::name

// 3.ASM damageinit: starts the damage bars (one green segment, or what we had if we are coming
// back to a damaged plane) and the bombs on the panel
void Main3::damageinit() {
    if (B(M3(invincible)) != 0) {
        drawbutton(0x0700, 13);   // white out the damage box
    } else {
        for (int cx = 4; cx; cx--) {
            const int bx = cx - 1;   // damage indicator 0-3
            int count = m_inst.damindicinit(bx);
            if (B(M3(redamageflag)) == 0) count = 1;   // show one segment of damage
            for (; count > 0; count--) drawdamage(bx);
        }
    }
    for (int cx = 3; cx; cx--) drawbutton(0x0807, 6 + (3 - cx));   // the three bombs appear
}

void Main3::drawflapbut() {
    drawbutton(0x0f07, 2);   // turn the letters white
    drawbutton(0x0600, 2);   // turn the button brown
}

void Main3::drawautobut() {
    drawbutton(0x0f07, 0);   // assume it is off, being turned on
    drawbutton(0x0100, 0);   // black becomes blue
}

void Main3::drawcmpltbut() {
    drawbutton(0x0f07, 5);
    drawbutton(0x0900, 5);
}

void Main3::drawabomb(int number) {   // bomb 0-2 appears
    drawbutton(0x0807, 6 + number);
}

// 3.ASM redrawbuts: after the panel was redrawn
void Main3::redrawbuts() {
    if (B(M3(flapflag)) != 0) drawflapbut();
    if (B(M3(autoflag)) != 0) drawautobut();
    if (B(M3(cmpltflag)) != 0) drawcmpltbut();
    const int missing = 3 - B(M3(nbombs));
    for (int cx = missing; cx > 0; cx--) drawbutton(0x0708, 6 + (3 - cx));   // the bombs we don't have disappear
}

// 3.ASM flashalt: the low altitude button
void Main3::flashalt() {
    drawbutton(0x0f00, 9);
    if (!(m_clock.oticks & 128)) {
        drawbutton(0x0f00, 9);
        drawbutton(0x070e, 9);
    } else {
        drawbutton(0x000f, 9);
        drawbutton(0x0e07, 9);
    }
}

// 3.ASM flasheject
void Main3::flasheject() {
    drawbutton(0x0400, 4);
    if (!(m_clock.oticks & 512)) {
        drawbutton(0x0400, 4);
        drawbutton(0x070f, 4);
    } else {
        drawbutton(0x0004, 4);
        drawbutton(0x0f07, 4);
    }
}

// 3.ASM flashstall: the stall light flickers when close to a stall
void Main3::flashstall() {
    if (m_rand.rand_w() > 16000) {   // leave the light out most of the time
        drawbutton(0x0e07, 3);
        drawbutton(0x0c00, 3);
    } else {
        drawbutton(0x070e, 3);
        drawbutton(0x000c, 3);
    }
}

} // namespace corncob::game
