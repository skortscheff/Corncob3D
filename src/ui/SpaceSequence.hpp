// SpaceSequence.hpp - Lost in space: what SRATS.EXE showed when the bomber climbed too high
//
// The sky fades to black over 144 frames (eight seconds at the timer's 18 per second) while the Earth shrinks away
// below; from half way stars come streaming towards the viewer. SRATS.ASM draws it with the EGA tricks of the
// time; this is the same picture on the frame buffer. A key skips it.

#pragma once
#include "platform/Window.hpp"

namespace corncob {

// Returns false when the window was closed. When shotPath is set it renders the middle of the sequence to that
// BMP file and returns at once (for checking).
bool runSpaceSequence(Window& window, const char* shotPath = nullptr);

} // namespace corncob
