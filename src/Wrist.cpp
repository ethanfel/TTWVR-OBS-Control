#include "Wrist.h"
#include "Panel.h"
#include "Trace.h"
#include "Preview.h"
#include <atomic>
#include <cstring>

namespace wrist {
namespace {
using Draw=void(__cdecl*)(Surface*,void*,void*,void*,void*,void*,void*,void*);
using Step=bool(__cdecl*)(int);
using Section=void(__cdecl*)(int);
using Input=void(__cdecl*)(void*,void*,int,int,int,int);
Draw nativeDraw;
Step nativeStep;
Section nativeSection;
Input nativeInput;
using RedrawDue=bool(__cdecl*)();
RedrawDue nativeRedrawDue;
std::atomic<bool> redrawPending{false};
BYTE* host;
std::atomic<bool> active{false};
std::atomic<unsigned> painted{0};
bool armed=false;
int previousButtons=0;
unsigned lastNotice=0;
studio::Picture previousPicture;
constexpr unsigned Ready=1, Record=2, Live=4;
int page() { return *reinterpret_cast<const int*>(host+0x124aa20); }
int category() { return *reinterpret_cast<const int*>(host+0xf7d12c); }
void refresh() {
    redrawPending=true;
}
bool __cdecl redrawDue() {
    const bool due=nativeRedrawDue();
    const bool pending=redrawPending.exchange(false);
    return due || pending;
}
unsigned flags(const studio::Status& s) {
    return (s.link==studio::Link::Ready && !s.busy ? Ready:0u) |
           (s.recording ? Record:0u) | (s.live ? Live:0u);
}
void __cdecl section(int next) {
    if (page()!=next) reset();
    nativeSection(next);
}
bool __cdecl step(int direction) {
    if (page()!=2 || (direction!=1 && direction!=-1)) return nativeStep(direction);
    const int before=category();
    if (active) {
        // During OBS the native index stays at the edge reached by the original wrap.
        const int destination=direction>0 ? 0 : 4;
        if (before!=destination && !nativeStep(direction)) return false;
        reset(); refresh(); return true;
    }
    bool moved=nativeStep(direction);
    if (moved && ((direction==1 && before==4 && category()==0) ||
                  (direction==-1 && before==0 && category()==4))) {
        armed=false; painted=0; lastNotice=0; active=true; studio::visit(); refresh();
    }
    return moved;
}
void __cdecl draw(Surface* surface,void* model,void* a,void* b,void* c,void* d,void* e,void* f) {
    if (active && surface) {
        studio::visit();
        auto s=studio::read();
        auto frame=studio::preview();
        if (drawObsPanel(*surface,s,frame)) {
            static thread_local int prior=-1;
            int current=frame ? 1 : 0;
            if (current!=prior) { obsTrace(frame ? "WristBoy Program frame drawn" : "WristBoy waiting for frame"); prior=current; }
            painted=flags(s); return;
        }
        painted=0;
    }
    nativeDraw(surface,model,a,b,c,d,e,f);
}
void __cdecl input(void* owner,void* frame,int a,int b,int x,int y) {
    if (!active) { nativeInput(owner,frame,a,b,x,y); return; }
    studio::visit();
    auto s=studio::read();
    unsigned notice=flags(s) | (static_cast<unsigned>(s.link)<<4) |
                    (s.busy<<9) | (s.rejected<<10) | (s.paused<<11) | (s.reconnecting<<12);
    auto picture=studio::preview();
    if (notice!=lastNotice || picture!=previousPicture) {
        lastNotice=notice; previousPicture=std::move(picture); refresh();
    }
    int buttons=(static_cast<unsigned char>(a)!=0) | ((static_cast<unsigned char>(x)!=0)<<1);
    if (!buttons) armed=true;
    const int pressed=buttons & ~previousButtons;
    previousButtons=buttons;
    unsigned shown=painted.load();
    if (armed && (shown&Ready) && buttons!=3) {
        if (pressed==1) studio::press(studio::Action::Record,shown&Record);
        if (pressed==2) studio::press(studio::Action::Stream,shown&Live);
    }
}
struct Call { unsigned site,target; const void* replacement; };
}
void reset() { active=false; painted=0; armed=false; previousButtons=0; }
bool install(HMODULE mod) {
    if (!mod) return false;
    host=reinterpret_cast<BYTE*>(mod);
    auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(host+reinterpret_cast<IMAGE_DOS_HEADER*>(host)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp!=0x6afa9b4f || nt->OptionalHeader.SizeOfImage!=0x1d84000) return false;
    const Call calls[]={{0x12bc66,0x12f6d0,reinterpret_cast<void*>(draw)},
        {0x13ca91,0x13c270,reinterpret_cast<void*>(step)}, {0x13cb88,0x13d8c0,reinterpret_cast<void*>(input)},
        {0x13ca61,0x13af90,reinterpret_cast<void*>(section)},
        {0x101bfe,0x100700,reinterpret_cast<void*>(redrawDue)}};
    constexpr unsigned sites=sizeof(calls)/sizeof(calls[0]);
    DWORD old[sites]{};
    for (const auto& c:calls) {
        auto* p=host+c.site;
        int distance; std::memcpy(&distance,p+1,4);
        if (*p!=0xe8 || p+5+distance!=host+c.target) return false;
    }
    nativeDraw=reinterpret_cast<Draw>(host+calls[0].target);
    nativeStep=reinterpret_cast<Step>(host+calls[1].target);
    nativeInput=reinterpret_cast<Input>(host+calls[2].target);
    nativeSection=reinterpret_cast<Section>(host+calls[3].target);
    nativeRedrawDue=reinterpret_cast<RedrawDue>(host+calls[4].target);
    // Acquire permission for all sites before changing any. Two share a page,
    // so unwind their protections in reverse order.
    unsigned count=0;
    for (;count<sites;++count)
        if (!VirtualProtect(host+calls[count].site,5,PAGE_EXECUTE_READWRITE,&old[count])) break;
    const bool ok=count==sites;
    if (ok) for (const auto& c:calls) {
        auto* p=host+c.site;
        auto delta=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(c.replacement)-reinterpret_cast<uintptr_t>(p+5));
        std::memcpy(p+1,&delta,4);
        FlushInstructionCache(GetCurrentProcess(),p,5);
    }
    while (count) { --count; DWORD unused; VirtualProtect(host+calls[count].site,5,old[count],&unused); }
    return ok;
}
}
