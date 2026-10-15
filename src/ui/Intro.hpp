// Intro.hpp - Title sequence, intro story, and the loading screen
//
// Original flow (CORNCOB.BAT): cc3d1.com (text splash) -> moag.exe (menus, runs
// cctitle.exe for the title and story) -> 3.exe (game). cctitle.exe and cc3d1.com
// ship without source and are packed, so the title layout/timing here is a
// recreation built from the original artwork (VGA\*.VGA) and the story script
// (TPICT.DAT), which are used as-is. The loading screen follows 3.ASM: the
// "Please wait one moment....." message, then the instrument panel (IMG\3D.IMG)
// fading in (fadeflag/setcolors).

#pragma once
#include "../platform/Window.hpp"

namespace corncob {

enum class IntroResult { Continue, Quit };

// When shotPrefix is set the sequence runs unattended and saves one BMP per
// scene as <shotPrefix>NN.bmp.
IntroResult runIntro(Window& window, const char* shotPrefix = nullptr);
IntroResult runLoadingScreen(Window& window, const char* shotPrefix = nullptr);

} // namespace corncob
