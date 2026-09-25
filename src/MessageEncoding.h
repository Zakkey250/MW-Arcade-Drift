#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace tire {
// Inverse of retail Language::ConvertToUCS2 (0x56BFC0).
inline bool Encode(const wchar_t* text, const std::uint16_t* table,
                   std::size_t count, std::string& out) {
    out.clear();
    if (!table || count < 256 || count > 32768) return false;
    for (; *text; ++text) {
        auto c = static_cast<std::uint16_t>(*text);
        if (c < 128) { out.push_back(static_cast<char>(c)); continue; }
        bool found = false;
        for (unsigned b = 128; b < 256 && !found; ++b) {
            if (table[b] == c) { out.push_back(static_cast<char>(b)); found = true; }
            else if (table[b] > 0 && table[b] < 128) {
                for (unsigned tail = 128; tail < 256; ++tail) {
                    auto index = (static_cast<unsigned>(table[b]) << 7) + tail - 128;
                    if (index < count && table[index] == c) {
                        out.push_back(static_cast<char>(b));
                        out.push_back(static_cast<char>(tail)); found = true; break;
                    }
                }
            }
        }
        if (!found) { out.clear(); return false; }
    }
    return true;
}
}
