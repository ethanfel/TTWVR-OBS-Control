#include "Preview.h"
#include "Trace.h"
#include <windows.h>
#include <d3d9.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>
#include <string_view>

namespace {
using Microsoft::WRL::ComPtr;
// FalloutNV.exe 1.4.0.525. These fields are Tile, TileImage, NiSourceTexture
// and NiDX9TextureData from the JIP LN / xNVSE runtime layouts.
template<class T> T field(const void* object,std::size_t offset) {
    T result;
    std::memcpy(&result,static_cast<const BYTE*>(object)+offset,sizeof(result));
    return result;
}
struct Child { Child* next; Child* previous; void* tile; };
void* child(const void* tile,const char* name) {
    if (!tile) return nullptr;
    for (auto* n=field<Child*>(tile,4); n; n=n->next) {
        const auto* text=n->tile ? field<const char*>(n->tile,0x20) : nullptr;
        if (text && !_stricmp(text,name)) return n->tile;
    }
    return nullptr;
}
bool isMonitorPath(const char* path) {
    if (!path) return false;
    std::string_view p(path);
    constexpr std::string_view suffix="ttwvrobs/monitor.dds";
    if (p.size()<suffix.size()) return false;
    p.remove_prefix(p.size()-suffix.size());
    for (size_t i=0;i<p.size();++i) {
        char c=p[i]=='\\' ? '/' : p[i];
        if (c>='A' && c<='Z') c+=32;
        if (c!=suffix[i]) return false;
    }
    return true;
}
IDirect3DTexture9* monitorTexture(const char*& reason) {
    reason="PauseBoy waiting for monitor tile";
    auto roots=*reinterpret_cast<void***>(0x11F350C);
    if (!roots) return nullptr;
    auto* panel=child(child(roots[1023-1001],"GLOW_BRANCH"),"TTWVROBS");
    auto* image=child(child(panel,"Body"),"Monitor");
    if (!image || field<DWORD>(image,0)!=0x106F01C) return nullptr;
    reason="PauseBoy waiting for monitor material";
    auto* shader=field<void*>(image,0x40);
    auto* bound=(shader && field<DWORD>(shader,0)==0x10B9D28) ? field<void*>(shader,0x60) : nullptr;
    // The material may already have its diffuse image while the tile cache is still empty.
    for (auto* tex : {bound,field<void*>(image,0x3c)}) {
        if (!tex || field<DWORD>(tex,0)!=0x109B9EC ||
            !(isMonitorPath(field<const char*>(tex,0x30)) || isMonitorPath(field<const char*>(tex,0x34)))) continue;
        reason="PauseBoy waiting for resident monitor texture";
        auto* data=field<void*>(tex,0x24);
        auto* base=data ? field<IDirect3DBaseTexture9*>(data,0x64) : nullptr;
        if (base && base->GetType()==D3DRTYPE_TEXTURE) return static_cast<IDirect3DTexture9*>(base);
    }
    return nullptr;
}
bool upload(IDirect3DTexture9* tex,const studio::Frame& frame) {
    for (UINT level=0;level<tex->GetLevelCount();++level) {
        D3DSURFACE_DESC d{};
        if (FAILED(tex->GetLevelDesc(level,&d)) || d.Width>1024 || d.Height>1024 ||
            (d.Format!=D3DFMT_A8R8G8B8 && d.Format!=D3DFMT_X8R8G8B8)) return false;
        D3DLOCKED_RECT rect{};
        if (SUCCEEDED(tex->LockRect(level,&rect,nullptr,0))) {
            studio::paintPreview(static_cast<std::uint32_t*>(rect.pBits),rect.Pitch/4,d.Width,d.Height,16,9,frame,false);
            if (FAILED(tex->UnlockRect(level))) return false;
            continue;
        }
        ComPtr<IDirect3DDevice9> device;
        ComPtr<IDirect3DSurface9> src,dst;
        if (d.Pool!=D3DPOOL_DEFAULT || FAILED(tex->GetDevice(&device)) ||
            FAILED(device->CreateOffscreenPlainSurface(d.Width,d.Height,d.Format,D3DPOOL_SYSTEMMEM,&src,nullptr)) ||
            FAILED(tex->GetSurfaceLevel(level,&dst)) || FAILED(src->LockRect(&rect,nullptr,0))) return false;
        studio::paintPreview(static_cast<std::uint32_t*>(rect.pBits),rect.Pitch/4,d.Width,d.Height,16,9,frame,false);
        if (FAILED(src->UnlockRect()) || FAILED(device->UpdateSurface(src.Get(),nullptr,dst.Get(),nullptr))) return false;
    }
    return true;
}
}
// Only called on the NVSE main loop, while our PauseBoy page is visible.
bool updatePausePreview(const studio::Picture& frame) {
    static studio::Picture lastFrame;
    static IDirect3DTexture9* lastTexture=nullptr;
    if (!frame) { lastFrame.reset(); lastTexture=nullptr; return false; }
    const char* stage=nullptr;
    auto* texture=monitorTexture(stage);
    static const char* lastStage=nullptr;
    auto report=[&](const char* message) {
        if (message!=lastStage) { obsTrace(message); lastStage=message; }
    };
    if (!texture) { report(stage); return false; }
    if (texture==lastTexture && lastFrame==frame) return true;
    if (!upload(texture,*frame)) { report("PauseBoy monitor upload failed"); return false; }
    report("PauseBoy Program frame uploaded");
    lastTexture=texture; lastFrame=frame;
    return true;
}
