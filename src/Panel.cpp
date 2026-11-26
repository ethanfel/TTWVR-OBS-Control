#include "Panel.h"
#include "Preview.h"
#include <windows.h>
#include <cstring>
#include <string>
#include <vector>

namespace {
// TTWVR supplies a tightly packed BGRA surface already cleared in the player's Pip-Boy colour.
struct Caption { int x, y, height; std::string text; };
struct Mask {
    static constexpr int w=512, h=320;
    std::vector<unsigned char> intensity;
    std::string key;
    bool prepare(const studio::Status& s,bool picture) {
        std::string id = std::string(studio::statusLine(s)) + "|" + studio::streamButton(s) + "|" + studio::recordButton(s) + "|" + studio::replayButton(s) + (picture ? "1" : "0");
        if (id == key) return true;
        BITMAPINFO info{};
        info.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB, 0, 0, 0, 0, 0};
        HDC dc=CreateCompatibleDC(nullptr);
        if (!dc) return false;
        void* bits=nullptr;
        auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
        if (!bitmap) { DeleteDC(dc); return false; }
        auto previous=SelectObject(dc,bitmap);
        std::memset(bits,0,w*h*4);
        SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255));
        const Caption captions[] = {
            {20,7,20,"OBS PROGRAM"}, {20,202,21,studio::statusLine(s)},
            {22,227,20,std::string("A   ")+studio::recordButton(s)},
            {22,250,20,std::string("X   ")+studio::streamButton(s)},
            {22,273,20,std::string("Y   ")+studio::replayButton(s)},
            {20,300,15,"<  RADIO                            LOCAL MAP  >"},
            {141,98,16,picture ? "" : "Preview unavailable"}
        };
        for (const auto& c:captions) {
            auto font=CreateFontA(-c.height,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FIXED_PITCH,"Consolas");
            auto old=SelectObject(dc,font);
            TextOutA(dc,c.x,c.y,c.text.c_str(),static_cast<int>(c.text.size()));
            SelectObject(dc,old); DeleteObject(font);
        }
        RECT line{20,197,492,198};
        FillRect(dc,&line,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        GdiFlush();
        intensity.resize(w*h);
        auto* src=static_cast<unsigned char*>(bits);
        for (int i=0; i<w*h; ++i) intensity[i]=src[i*4];
        SelectObject(dc,previous); DeleteObject(bitmap); DeleteDC(dc);
        key=std::move(id);
        return true;
    }
};
}
bool drawObsPanel(Surface& dst,const studio::Status& s,const studio::Picture& picture) {
    if (!dst.pixels || dst.width<128 || dst.height<96 || dst.width>2048 || dst.height>2048) return false;
    static thread_local Mask mask;
    if (!mask.prepare(s,bool(picture))) return false;
    if (picture) {
        const int x=104*dst.width/512, y=28*dst.height/320;
        const int w=304*dst.width/512, h=166*dst.height/320;
        studio::paintPreview(dst.pixels+y*dst.width+x,dst.width,w,h,304,166,*picture,true);
    }
    for (int y=0; y<dst.height; ++y) {
        auto* row=mask.intensity.data()+(y*Mask::h/dst.height)*Mask::w;
        for (int x=0; x<dst.width; ++x) {
            unsigned ink=row[x*Mask::w/dst.width];
            if (!ink) continue;
            auto& pixel=dst.pixels[y*dst.width+x];
            // Composite glyphs on TTWVR's background, keeping its screen alpha intact.
            unsigned inverse=255-ink;
            unsigned r=(((pixel>>16)&255)*inverse+145*ink)/255;
            unsigned g=(((pixel>>8)&255)*inverse+255*ink)/255;
            unsigned b=((pixel&255)*inverse+169*ink)/255;
            pixel=(pixel&0xff000000u)|(r<<16)|(g<<8)|b;
        }
    }
    return true;
}
