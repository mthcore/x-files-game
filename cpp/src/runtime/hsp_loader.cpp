// SPDX-License-Identifier: MIT
#include "runtime/hsp_loader.h"

#include <cstring>
#include <cstdio>

namespace xfiles::runtime {

namespace {
inline uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
inline int16_t le16(const uint8_t* p) {
    return int16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8));
}
}  // namespace

std::vector<Hotspot> load_hot_bytes(const std::vector<uint8_t>& b) {
    std::vector<Hotspot> out;
    if (b.size() < 8 || std::memcmp(b.data(), "HSPT", 4) != 0) return out;
    uint32_t n = le32(&b[4]);
    if (b.size() != 8u + n * 20u) return out;
    out.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t* e = &b[8 + i * 20];
        Hotspot h;
        h.index = int(i);
        h.type_zorder = le32(e);
        h.x_min = le16(e + 4);
        h.y_min = le16(e + 6);
        h.x_max = le16(e + 8);
        h.y_max = le16(e + 10);
        h.action_id_1 = le32(e + 12);
        h.action_id_2 = le32(e + 16);
        out.push_back(h);
    }
    return out;
}

std::vector<Hotspot> load_hot_file(const std::string& path) {
    std::vector<uint8_t> buf;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return {};
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size > 0) {
        buf.resize(static_cast<std::size_t>(size));
        if (std::fread(buf.data(), 1, buf.size(), f) != buf.size()) {
            buf.clear();
        }
    }
    std::fclose(f);
    return load_hot_bytes(buf);
}

int hit_test(const std::vector<Hotspot>& rects, int x, int y) {
    for (std::size_t i = 0; i < rects.size(); ++i) {
        const Hotspot& h = rects[i];
        if (x >= h.x_min && x < h.x_max && y >= h.y_min && y < h.y_max) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}  // namespace xfiles::runtime
