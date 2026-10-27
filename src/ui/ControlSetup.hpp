// ControlSetup.hpp - The keyboard/joystick setup menu on its own
//
// The same menu the front end has, opened by the Start button during a mission. Changes are saved to the
// settings; the key choices are applied at once.

#pragma once

namespace corncob {

class Window;
struct Settings;

void runControlSetup(Window& window, Settings& settings);

} // namespace corncob
