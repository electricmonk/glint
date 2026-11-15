// SPDX-License-Identifier: AGPL-3.0-or-later
// Tiny WAV reader (PCM 16/24/32, float 32) and writer (float 32).
#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace glint::wav {

struct Audio {
    double sampleRate = 48000.0;
    std::vector<float> l, r;
    size_t size() const { return l.size(); }
};

inline bool write(const std::string& path, const Audio& a)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    const uint32_t n = static_cast<uint32_t>(a.size());
    const uint32_t dataBytes = n * 2 * 4;
    auto u32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
    auto u16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
    f.write("RIFF", 4); u32(36 + dataBytes); f.write("WAVE", 4);
    f.write("fmt ", 4); u32(16); u16(3); u16(2);
    u32(static_cast<uint32_t>(a.sampleRate)); u32(static_cast<uint32_t>(a.sampleRate) * 8); u16(8); u16(32);
    f.write("data", 4); u32(dataBytes);
    for (size_t i = 0; i < a.size(); ++i) {
        f.write(reinterpret_cast<const char*>(&a.l[i]), 4);
        f.write(reinterpret_cast<const char*>(&a.r[i]), 4);
    }
    return static_cast<bool>(f);
}

inline bool read(const std::string& path, Audio& a)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) != 0 || std::memcmp(d.data() + 8, "WAVE", 4) != 0)
        return false;
    uint16_t fmt = 0, ch = 0, bits = 0;
    uint32_t sr = 0;
    size_t p = 12;
    while (p + 8 <= d.size()) {
        uint32_t sz;
        std::memcpy(&sz, d.data() + p + 4, 4);
        const char* id = d.data() + p;
        const char* body = d.data() + p + 8;
        if (std::memcmp(id, "fmt ", 4) == 0) {
            std::memcpy(&fmt, body, 2);
            std::memcpy(&ch, body + 2, 2);
            std::memcpy(&sr, body + 4, 4);
            std::memcpy(&bits, body + 14, 2);
            if (fmt == 0xFFFE) std::memcpy(&fmt, body + 24, 2);
        } else if (std::memcmp(id, "data", 4) == 0) {
            const size_t bytes = bits / 8;
            const size_t frames = sz / (bytes * ch);
            a.sampleRate = sr;
            a.l.resize(frames);
            a.r.resize(frames);
            for (size_t i = 0; i < frames; ++i) {
                float s[2] = { 0, 0 };
                for (size_t c = 0; c < ch && c < 2; ++c) {
                    const char* q = body + (i * ch + c) * bytes;
                    if (fmt == 3 && bits == 32) { std::memcpy(&s[c], q, 4); }
                    else if (bits == 16) { int16_t v; std::memcpy(&v, q, 2); s[c] = v / 32768.0f; }
                    else if (bits == 24) { int32_t v = (static_cast<uint8_t>(q[0]) | (static_cast<uint8_t>(q[1]) << 8) | (static_cast<int8_t>(q[2]) << 16)); s[c] = v / 8388608.0f; }
                    else if (bits == 32) { int32_t v; std::memcpy(&v, q, 4); s[c] = static_cast<float>(v / 2147483648.0); }
                }
                a.l[i] = s[0];
                a.r[i] = ch > 1 ? s[1] : s[0];
            }
            return true;
        }
        p += 8 + sz + (sz & 1);
    }
    return false;
}

} // namespace glint::wav
