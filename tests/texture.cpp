// Exercise the same D3D9 upload used by Fallout, on a real Windows device.
#include "../src/PausePreview.cpp"
#include <cstdio>

int main() {
    ComPtr<IDirect3D9> api;
    api.Attach(Direct3DCreate9(D3D_SDK_VERSION));
    if (!api) return 1;
    HWND window=CreateWindowA("STATIC","OBS preview test",WS_POPUP,0,0,32,32,nullptr,nullptr,nullptr,nullptr);
    D3DPRESENT_PARAMETERS params{};
    params.Windowed=TRUE;
    params.SwapEffect=D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow=window;
    params.BackBufferWidth=32; params.BackBufferHeight=32;
    ComPtr<IDirect3DDevice9> device;
    if (FAILED(api->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&params,&device))) return 2;
    ComPtr<IDirect3DTexture9> tex;
    if (FAILED(device->CreateTexture(256,128,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&tex,nullptr))) return 3;
    studio::Frame frame{2,2,{0xffcc2200,0xff008811,0xff2244ff,0xffffffff}};
    if (!upload(tex.Get(),frame)) return 4;
    D3DLOCKED_RECT r{};
    if (FAILED(tex->LockRect(0,&r,nullptr,D3DLOCK_READONLY))) return 5;
    const auto* src=static_cast<const std::uint32_t*>(r.pBits);
    bool match=src[8]==0xff000000 && src[80]==frame.pixels[0] && src[175]==frame.pixels[1]
              && src[(r.Pitch/4)*96+80]==frame.pixels[2];
    tex->UnlockRect(0);
    if (!match) return 6;
    ComPtr<IDirect3DTexture9> defaultTexture;
    if (FAILED(device->CreateTexture(256,128,1,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&defaultTexture,nullptr))
        || !upload(defaultTexture.Get(),frame)) return 7;
    std::puts("PauseBoy D3D9 upload verified in managed and default pools, with letterbox and colour readback.");
    return 0;
}
