#pragma once
#include <windows.h>
#include <cstdio>

// Call on a state transition only. No scene names, image data, or credentials.
inline void obsTrace(const char* message) {
    if (auto* f=std::fopen("Data/NVSE/Plugins/TTWVROBS.log","a")) {
        std::fprintf(f,"%llu %s\n",GetTickCount64(),message);
        std::fclose(f);
    }
}
