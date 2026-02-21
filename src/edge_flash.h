#pragma once
#include <windows.h>

namespace edge_flash {

bool init(HINSTANCE hInstance);
void flash();
void shutdown();

}  // namespace edge_flash
