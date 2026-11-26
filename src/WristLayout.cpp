#include "WristLayout.h"
#include <cstdint>
#include <cstring>
#include <string_view>

namespace wrist {
namespace {
using namespace std::string_view_literals;

std::string_view sectionBytes(HMODULE module, const char* name, DWORD flags) {
    auto* base = reinterpret_cast<const char*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return {};
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return {};
    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        if (std::strncmp(reinterpret_cast<const char*>(section->Name), name, IMAGE_SIZEOF_SHORT_NAME) != 0 ||
            (section->Characteristics & flags) != flags) continue;
        const auto size = nt->OptionalHeader.SizeOfImage;
        if (section->VirtualAddress > size || section->Misc.VirtualSize > size - section->VirtualAddress) return {};
        return {base + section->VirtualAddress, section->Misc.VirtualSize};
    }
    return {};
}

bool matches(std::string_view code, std::size_t offset, std::string_view bytes) {
    return offset <= code.size() && code.substr(offset, bytes.size()) == bytes;
}

template<class T> T operand(const char* address) {
    T value;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

// A call must have both the expected surrounding instructions and the expected
// callee. Multiple matches mean we cannot identify this menu operation.
MenuCall findCall(std::string_view code, std::string_view anchor, int delta, std::string_view callee) {
    MenuCall result;
    for (auto pos = code.find(anchor); pos != code.npos; pos = code.find(anchor, pos + 1)) {
        const auto site = static_cast<std::int64_t>(pos) + delta;
        if (site < 0 || site + 5 > static_cast<std::int64_t>(code.size()) || BYTE(code[site]) != 0xe8) continue;
        const auto target = site + 5 + operand<std::int32_t>(code.data() + site + 1);
        if (target < 0 || target >= static_cast<std::int64_t>(code.size()) ||
            !matches(code, static_cast<std::size_t>(target), callee)) continue;
        if (result.site) return {};
        auto* base = reinterpret_cast<BYTE*>(const_cast<char*>(code.data()));
        result = {base + site, base + target};
    }
    return result;
}

const int* menuValue(std::string_view data, const char* load, const char* store) {
    auto addr = operand<std::uintptr_t>(load);
    auto base = reinterpret_cast<std::uintptr_t>(data.data());
    if (addr != operand<std::uintptr_t>(store) || addr % alignof(int) != 0 ||
        addr < base || data.size() < sizeof(int) || addr - base > data.size() - sizeof(int)) return nullptr;
    return reinterpret_cast<const int*>(addr);
}
}

bool findMenuLayout(HMODULE module, MenuLayout& layout) {
    if (!module) return false;
    auto code = sectionBytes(module, ".text", IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ);
    auto data = sectionBytes(module, ".data", IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_READ);
    if (code.empty() || data.empty()) return false;
    MenuLayout found;
    // DATA takes eight draw arguments. The adjacent inventory draw shares its prologue.
    found.draw = findCall(code, "\x8b\x06\x2b\x03\xc1\xf8\x07\x50"sv, -13,
                         "\x55\x8b\xec\x83\xe4\xc0\x81\xec\xf4\x0c\x00\x00"sv);
    // Step passes -1 or +1. Input dispatches four buttons only on the DATA page.
    found.step = findCall(code, "\x33\xc0\x85\xf6\x0f\x95\xc0\x8d\x04\x45\xff\xff\xff\xff\x50"sv, 15,
                         "\x56\x57\xff\x15"sv);
    found.input = findCall(code, "\x83\xf8\x02\x75\x15\xff\x75\xd0\xff\x75\xd4\xff\x75\xdc\xff\x75\xeb\x56\x51"sv, 19,
                          "\x81\xec\xe4\x01\x00\x00\xa1"sv);
    // Section uses the three-page table. The redraw predicate gates the screen refresh.
    found.section = findCall(code, "\x83\xc1\x02\x03\xc1\x33\xd2\xb9\x03\x00\x00\x00\xf7\xf1\x8b\x04\x95"sv, 22,
                            "\x56\x8b\x74\x24\x08\x3b\x35"sv);
    found.redraw = findCall(code, "\x75\x09\x55\xe8"sv, 11,
                           "\x83\xec\x18\x53\x55\x56\x57\x8d\x44\x24\x18\x50\xff\x15"sv);
    if (!found.draw.site || !found.step.site || !found.input.site || !found.section.site || !found.redraw.site) return false;

    auto index = [&](const BYTE* p) { return static_cast<std::size_t>(reinterpret_cast<const char*>(p) - code.data()); };
    auto draw = index(found.draw.site), redraw = index(found.redraw.site);
    auto step = index(found.step.target), section = index(found.section.target);
    if (draw < 6 || !matches(code, draw - 6, "\x8d\x44\x24\x50\x53\x50"sv) ||
        !matches(code, draw + 5, "\x83\xc4\x20\xe9"sv) ||
        !matches(code, redraw + 5, "\x84\xc0\x0f\x84"sv) ||
        !matches(code, redraw + 13, "\x68\x00\x00\x0c\x00"sv)) return false;

    // The same page is read by step and stored by section. The DATA category is
    // read and written around the five-category wrap; use the relocated operands.
    if (!matches(code, step + 8, "\x8b\x0d"sv) || !matches(code, section + 0x17, "\x89\x35"sv) ||
        !matches(code, section + 0x1d, "\xc7\x05"sv) || !matches(code, step + 0xae, "\x8b\x0d"sv) ||
        !matches(code, step + 0xb4, "\x74\x05\x83\xf9\x04\x74\x5c\x8b\x44\x24\x0c\x83\xc1\x05"sv) ||
        !matches(code, step + 0xcc, "\xb9\x05\x00\x00\x00\xf7\xf9"sv) ||
        !matches(code, step + 0xda, "\x89\x15"sv) || !matches(code, step + 0xe0, "\x83\xfa\x01"sv)) return false;
    found.page = menuValue(data, code.data() + step + 0xa, code.data() + section + 0x19);
    found.category = menuValue(data, code.data() + step + 0xb0, code.data() + step + 0xdc);
    if (!found.page || !found.category || found.page == found.category) return false;
    layout = found;
    return true;
}
}
