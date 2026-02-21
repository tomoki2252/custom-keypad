#pragma once
#include <windows.h>

namespace indicator {

bool init(HINSTANCE hInstance);
void show();
void hide();
void shutdown();
RECT get_rect();

}  // namespace indicator
