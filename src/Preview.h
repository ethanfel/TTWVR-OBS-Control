#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace studio {
struct Frame {
    int width, height;
    std::vector<std::uint32_t> pixels;
};
using Picture = std::shared_ptr<const Frame>;
Picture decodePreview(const std::string& dataUrl);
Picture preview();
// Fit to the displayed aspect even when a UI texture has non-square texels.
void paintPreview(std::uint32_t* dst, int stride, int width, int height,
                  int displayWidth, int displayHeight, const Frame& frame, bool preserveAlpha);
}
