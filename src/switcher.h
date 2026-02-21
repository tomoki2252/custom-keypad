#pragma once
#include <windows.h>

namespace switcher {

bool init(HINSTANCE hInstance);
void toggle();    // Enumerate windows and show/refresh the list
void hide();
void shutdown();

}  // namespace switcher
