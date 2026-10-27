// ControlSetup.hpp - The keyboard/joystick setup menu on its own
//
// The same menu the front end has, opened by the Start button during a mission. Changes are saved to the
// settings; the key choices are applied at once.

#pragma once

namespace corncob {

class Window;
struct Settings;

// inMission: the menu ends with 'Return to the game' and 'Return to the main menu'; true comes back when the player chose the
// main menu (the mission is abandoned).
bool runControlSetup(Window& window, Settings& settings, bool inMission = false);

} // namespace corncob
