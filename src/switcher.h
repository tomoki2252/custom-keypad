#pragma once
#include <windows.h>

namespace switcher {

bool init(HINSTANCE hInstance);
void toggle();       // Enumerate + show/refresh list
void move_left();    // Move cursor left + focus
void move_right();   // Move cursor right + focus
void hide();
void shutdown();

}  // namespace switcher
