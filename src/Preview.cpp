#include "Preview.h"
#include <windows.h>
#include <wincrypt.h>
#include <cstring>
#include <algorithm>

namespace studio {

Picture decodePreview(const std::string& url) {
    const auto comma=url.find(',');
    if (comma==std::string::npos || comma>40 || url.size()>384*1024 ||
        url.compare(0,comma,"data:image/bmp;base64")!=0)
        return {};
    auto input=url.data()+comma+1;
    DWORD length=static_cast<DWORD>(url.size()-comma-1);
    DWORD bytes=0;
    if (!CryptStringToBinaryA(input,length,CRYPT_STRING_BASE64,nullptr,&bytes,nullptr,nullptr) || !bytes) return {};
    std::vector<BYTE> raw(bytes);
    if (!CryptStringToBinaryA(input,length,CRYPT_STRING_BASE64,raw.data(),&bytes,nullptr,nullptr)) return {};
    // Qt writes an uncompressed Windows DIB. Its scanlines are padded to four bytes.
    if (bytes<54 || raw[0]!='B' || raw[1]!='M') return {};
    auto u32=[&](unsigned at) { std::uint32_t n; std::memcpy(&n,raw.data()+at,4); return n; };
    auto u16=[&](unsigned at) { std::uint16_t n; std::memcpy(&n,raw.data()+at,2); return n; };
    const unsigned offset=u32(10),header=u32(14),w=u32(18),bits=u16(28);
    const auto signedHeight=static_cast<std::int32_t>(u32(22));
    if (header<40 || header>bytes-14 || offset<14+header || offset>bytes ||
        !w || w>512 || !signedHeight || signedHeight< -512 || signedHeight>512 ||
        u16(26)!=1 || (bits!=24 && bits!=32) || u32(30)!=0) return {};
    const unsigned h=static_cast<unsigned>(std::abs(signedHeight)),channels=bits/8;
    const unsigned pitch=(w*channels+3)&~3u;
    if (pitch*h>bytes-offset) return {};
    auto frame=std::make_shared<Frame>();
    frame->width=w; frame->height=h; frame->pixels.resize(w*h);
    for (unsigned y=0;y<h;++y) {
        const auto* row=raw.data()+offset+(signedHeight<0 ? y : h-1-y)*pitch;
        for (unsigned x=0;x<w;++x) {
            const auto* p=row+x*channels;
            frame->pixels[y*w+x]=0xff000000u|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[1])<<8)|p[0];
        }
    }
    return frame;
}

void paintPreview(std::uint32_t* dst,int stride,int width,int height,int displayWidth,int displayHeight,
                  const Frame& frame,bool preserveAlpha) {
    const double srcAspect=double(frame.width)/frame.height, dstAspect=double(displayWidth)/displayHeight;
    const int w=std::clamp(int(width*srcAspect/dstAspect+0.5),1,width);
    const int h=std::clamp(int(height*dstAspect/srcAspect+0.5),1,height);
    const int left=(width-w)/2,top=(height-h)/2;
    for (int y=0;y<height;++y) {
        auto* row=dst+y*stride;
        const auto* src=(y>=top && y<top+h) ? frame.pixels.data()+((y-top)*frame.height/h)*frame.width : nullptr;
        for (int x=0;x<width;++x) {
            const auto color=(src && x>=left && x<left+w) ? src[(x-left)*frame.width/w] : 0u;
            row[x]=(preserveAlpha ? row[x]&0xff000000u : 0xff000000u) | (color&0xffffff);
        }
    }
}
}
