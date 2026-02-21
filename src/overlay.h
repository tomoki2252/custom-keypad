#pragma once
#include <windows.h>
#include <string>

namespace overlay {

bool init(HINSTANCE hInstance);
void show(int x, int y, const std::wstring& text);
void hide();

}  // namespace overlay
