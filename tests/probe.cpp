#include "Control.h"
#include "Panel.h"
#include "Preview.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace studio;
template<class Predicate> bool waitFor(Predicate p) {
    auto until=GetTickCount64()+6500;
    do { visit(); if (p(read())) return true; Sleep(25); } while(GetTickCount64()<until);
    auto s=read(); std::fprintf(stderr,"timeout: %s\n",statusLine(s)); return false;
}
int main(int argc,char** argv) {
    if (argc==2 && !std::strcmp(argv[1],"--panel")) {
        std::vector<std::uint32_t> pixels(512*320,0xff061108);
        Surface surface{pixels.data(),512,320};
        Status s; s.link=Link::Ready; s.recording=true;
        if (!drawObsPanel(surface,s)) return 2;
        unsigned lit=0; for (auto p:pixels) { if ((p>>24)!=255) return 3; if (p!=0xff061108) ++lit; }
        std::printf("panel %u glyph pixels, preserved alpha\n",lit); return lit>500 ? 0 : 4;
    }
    if (!waitFor([](Status s){return s.link==Link::Ready;})) return 1;
    std::puts(statusLine(read()));
    bool fixture=argc==2 && !std::strcmp(argv[1],"--fixture") && GetFileAttributesW(L"mock-obs-only.flag")!=INVALID_FILE_ATTRIBUTES;
    if (fixture || (argc==2 && !std::strcmp(argv[1],"--preview"))) {
        if (!waitFor([](Status){return bool(preview());})) return 7;
        auto first=preview();
        if (!waitFor([&](Status){auto frame=preview();return frame && frame!=first;})) return 8;
        // Probe the picture at the centre of the wrist monitor and its game-provided alpha.
        std::vector<std::uint32_t> pixels(512*320,0x88010101);
        Surface surface{pixels.data(),512,320};
        if (!drawObsPanel(surface,read())) return 9;
        auto middle=pixels[111*512+256];
        if ((middle>>24)!=0x88 || (fixture && ((middle>>16)&255)<150)) return 10;
        std::printf("Verified consecutive %dx%d Program frames and wrist monitor rendering.\n",first->width,first->height);
    }
    // Production diagnostic is read-only. Only the test runner creates this fixture directory.
    if (argc!=2 || std::strcmp(argv[1],"--fixture") || GetFileAttributesW(L"mock-obs-only.flag")==INVALID_FILE_ATTRIBUTES) return 0;
    if (read().replay != Replay::Unavailable) return 11;
    press(Action::Replay,false); // Disabled in OBS, cannot send an action.
    press(Action::Record,false); press(Action::Record,false);
    if (!waitFor([](Status s){return s.recording && !s.busy;})) return 2;
    press(Action::Stream,false);
    if (!waitFor([](Status s){return s.live && s.recording && !s.busy;})) return 3;
    press(Action::Stream,false); // a stale button must not stop a stream
    press(Action::Record,true);
    if (!waitFor([](Status s){return s.live && !s.recording && !s.busy;})) return 4;
    press(Action::Stream,true);
    if (!waitFor([](Status s){return !s.live && !s.recording && !s.busy;})) return 5;
    press(Action::Record,false);
    if (!waitFor([](Status s){return s.rejected && !s.recording && !s.busy;})) return 6;
    if (read().replay != Replay::Stopped) return 12;
    press(Action::Replay,true); // Stale Save button must never start the buffer.
    press(Action::Replay,false);
    if (!waitFor([](Status s){return s.replay==Replay::Running && !s.busy;})) return 13;
    press(Action::Replay,false); // Stale Start button must never save.
    press(Action::Replay,true); press(Action::Replay,true);
    if (!waitFor([](Status s){return s.clip==Clip::Saving && s.busy;})) return 14;
    if (!waitFor([](Status s){return s.clip==Clip::Saved && !s.busy;})) return 15;
    press(Action::Replay,true);
    if (!waitFor([](Status s){return s.clip==Clip::Saved && !s.busy;})) return 16;
    press(Action::Replay,true);
    if (!waitFor([](Status s){return s.rejected && s.clip==Clip::Idle && !s.busy;})) return 17;
    stop(); Sleep(100); return 0;
}
