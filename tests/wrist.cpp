// Use an inert copy of the real TTWVR image. Its game entry points are never executed here.
#include "../src/Wrist.cpp"
#include <array>
#include <cstdio>
#include <cwchar>

namespace {
studio::Status status;
int draws, commands;
studio::Action pressed;
bool expectedActive;
bool __cdecl simulateStep(int direction) {
    auto* value=const_cast<int*>(wrist::menu.category);
    *value=(*value+5+direction)%5;
    return true;
}
void writeByte(BYTE* address,BYTE value) {
    DWORD old;
    if (!VirtualProtect(address,1,PAGE_EXECUTE_READWRITE,&old)) std::abort();
    *address=value;
    VirtualProtect(address,1,old,&old);
    FlushInstructionCache(GetCurrentProcess(),address,1);
}
BYTE* destination(BYTE* site) {
    std::int32_t delta;
    std::memcpy(&delta,site+1,4);
    return site+5+delta;
}
}
namespace studio {
void visit() {}
Status read() {return status;}
Picture preview() {return {};}
void press(Action action,bool active) {++commands; pressed=action; expectedActive=active;}
}
bool drawObsPanel(Surface&,const studio::Status&,const studio::Picture&) {++draws; return true;}

int wmain(int argc,wchar_t** argv) {
    if (argc!=3 || (std::wcscmp(argv[1],L"1.1.7") && std::wcscmp(argv[1],L"1.1.8"))) return 2;
    auto module=LoadLibraryExW(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    if (!module) {std::fprintf(stderr,"Could not map TTWVR: %lu\n",GetLastError()); return 3;}
    auto* base=reinterpret_cast<BYTE*>(module);
    wrist::MenuLayout m;
    if (!wrist::findMenuLayout(module,m)) {std::puts("Menu layout missing"); return 4;}
    const std::array<wrist::MenuCall,5> calls={m.draw,m.step,m.input,m.section,m.redraw};
    const unsigned oldSites[]={0x12bc66,0x13ca91,0x13cb88,0x13ca61,0x101bfe};
    const unsigned newSites[]={0x138056,0x149651,0x149748,0x149621,0x10be2e};
    const bool newer=!std::wcscmp(argv[1],L"1.1.8");
    for (size_t i=0;i<calls.size();++i)
        if (calls[i].site!=base+(newer?newSites:oldSites)[i] || destination(calls[i].site)!=calls[i].target) return 5;
    if (m.page!=reinterpret_cast<int*>(base+(newer?0x1456ed0:0x124aa20)) ||
        m.category!=reinterpret_cast<int*>(base+(newer?0x118812c:0xf7d12c))) return 6;

    // Model a changed redraw routine, then mismatched category read/write operands.
    for (auto* mismatch : {m.redraw.target,m.step.target+0xdc}) {
        BYTE saved=*mismatch;
        writeByte(mismatch,saved^1);
        if (wrist::install(module)) return 7;
        for (auto call:calls) if (destination(call.site)!=call.target) return 8;
        writeByte(mismatch,saved);
    }
    if (!wrist::install(module)) return 9;
    for (auto call:calls) if (destination(call.site)==call.target) return 10;
    wrist::nativeStep=simulateStep;
    *const_cast<int*>(m.page)=2;
    *const_cast<int*>(m.category)=4;
    auto step=reinterpret_cast<wrist::Step>(destination(m.step.site));
    auto draw=reinterpret_cast<wrist::Draw>(destination(m.draw.site));
    auto input=reinterpret_cast<wrist::Input>(destination(m.input.site));
    if (!step(1) || !wrist::active) return 11;
    status.link=studio::Link::Ready; status.replay=studio::Replay::Running;
    Surface surface{};
    draw(&surface,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr);
    if (draws!=1) return 12;
    for (int button:{0,1,2}) {
        input(nullptr,nullptr,0,0,0,0);
        input(nullptr,nullptr,button==0,0,button==1,button==2);
        studio::Action want[]={studio::Action::Record,studio::Action::Stream,studio::Action::Replay};
        if (commands!=button+1 || pressed!=want[button] || expectedActive!=(button==2)) return 13;
    }
    if (!step(-1) || wrist::active || *m.category!=4) return 14;
    std::printf("TTWVR %ls: five calls, tab navigation, A/X/Y routing and incompatible-layout fallback passed.\n",argv[1]);
    return 0;
}
