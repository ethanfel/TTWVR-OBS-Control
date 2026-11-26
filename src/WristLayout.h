#pragma once
#include <windows.h>

namespace wrist {
struct MenuCall {
    BYTE* site = nullptr;
    BYTE* target = nullptr;
};
struct MenuLayout {
    MenuCall draw, step, input, section, redraw;
    const int* page = nullptr;
    const int* category = nullptr;
};
bool findMenuLayout(HMODULE module, MenuLayout& layout);
}
