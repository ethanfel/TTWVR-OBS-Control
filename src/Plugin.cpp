#include <windows.h>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include "Control.h"
#include "Wrist.h"
#include "Preview.h"

bool updatePausePreview(const studio::Picture&);

namespace {
struct Nvse {
    unsigned version,runtime,editorVersion,editor; void* add; void* opcode;
    void* (__cdecl* query)(unsigned); unsigned (__cdecl* handle)();
};
struct Info { unsigned version; const char* name; unsigned build; };
struct Message { const char* sender; unsigned type, length; void* data; };
struct Messaging { unsigned version; bool (__cdecl* listen)(unsigned,const char*,void(__cdecl*)(Message*)); };
struct Result { double number{}; unsigned char type{}; };
static_assert(sizeof(Result)==16);
struct Scripts {
    bool (__cdecl* call)(void*,void*,void*,Result*,unsigned char,...);
    void *params,*extract,*format,*callAlt;
    void* (__cdecl* compile)(const char*);
};
Scripts* scripts;
void *init, *poll, *display;
bool initialized=false, inGame=false, dirty=true;
unsigned lastView=0;
ULONGLONG nextTick=0;
void log(const char* msg) {
    if (auto f=std::fopen("Data/NVSE/Plugins/TTWVROBS.log","a")) {
        std::fprintf(f,"%llu %s\n",GetTickCount64(),msg); std::fclose(f);
    }
}
void* compileUdf(const char* path) {
    const std::string filename=std::string("Data/NVSE/user_defined_functions/TTWVROBS/")+path+".txt";
    std::ifstream input(filename);
    if (!input) { log(filename.c_str()); return nullptr; }
    std::string program((std::istreambuf_iterator<char>(input)),{});
    auto* udf=scripts->compile(program.c_str());
    if (!udf) log(filename.c_str());
    return udf;
}
void initialize() {
    initialized=true;
    log(wrist::install(GetModuleHandleW(L"TTWVR.dll")) ? "WristBoy DATA / OBS installed" :
        "WristBoy unavailable: unsupported TTWVR build or menu layout");
    init=compileUdf("Init"); poll=compileUdf("Poll"); display=compileUdf("Display");
    Result r;
    if (!init || !poll || !display || !scripts->call(init,nullptr,nullptr,&r,0)) {
        poll=nullptr; log("PauseBoy unavailable: check installed UDF files and JIP LN"); return;
    }
    log("PauseBoy DATA / OBS registered (0.3.0)");
}
void __cdecl onMessage(Message* m) {
    if (!m) return;
    switch (m->type) {
    case 1: case 7: studio::stop(); return;
    case 2: case 6: inGame=false; dirty=true; wrist::reset(); return;
    case 8: inGame=m->data!=nullptr; dirty=true; return;
    case 14: inGame=true; dirty=true; return;
    case 20: break;
    default: return;
    }
    if (!inGame) return;
    if (!initialized) initialize();
    const auto now=GetTickCount64();
    if (!poll || now<nextTick) return;
    nextTick=now+90;
    Result r;
    if (!scripts->call(poll,nullptr,nullptr,&r,0) || r.type!=1 || r.number<1) { dirty=true; return; }
    studio::visit();
    int cmd=static_cast<int>(r.number);
    if (cmd==2 || cmd==3) studio::press(studio::Action::Record,cmd==3);
    if (cmd==4 || cmd==5) studio::press(studio::Action::Stream,cmd==5);
    if (cmd==6 || cmd==7) studio::press(studio::Action::Replay,cmd==7);
    auto s=studio::read();
    const bool picture=updatePausePreview(studio::preview());
    unsigned view=(unsigned(picture)<<10) | static_cast<unsigned>(s.link) | (s.recording<<4) | (s.live<<5) | (s.busy<<6) |
                  (s.paused<<7) | (s.reconnecting<<8) | (s.rejected<<9) |
                  (static_cast<unsigned>(s.replay)<<11) | (static_cast<unsigned>(s.clip)<<13);
    if (dirty || view!=lastView) {
        bool ok=scripts->call(display,nullptr,nullptr,&r,9,studio::statusLine(s),studio::recordButton(s),
                    studio::streamButton(s),int(s.link==studio::Link::Ready && !s.busy),int(s.recording),int(s.live),int(picture),
                    studio::replayButton(s),static_cast<int>(s.replay));
        dirty=!ok; lastView=view;
    }
}
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Query(const Nvse* n,Info* i) {
    if (!n || !i) return false;
    *i={1,"TTWVR OBS Control",4};
    return !n->editor && n->runtime==0x040020d0;
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Load(const Nvse* n) {
    scripts=static_cast<Scripts*>(n->query(6));
    auto* msg=static_cast<Messaging*>(n->query(2));
    return scripts && scripts->compile && scripts->call && msg && msg->listen(n->handle(),"NVSE",onMessage);
}
