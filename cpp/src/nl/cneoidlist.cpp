// SPDX-License-Identifier: MIT
// CNeoIDList byte-direct decoder implementation.

#include "nl/cneoidlist.h"

#include <cstring>

namespace xfiles {
namespace nl {

namespace {
uint32_t read_be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
         | (uint32_t(p[2]) <<  8) |  uint32_t(p[3]);
}
}  // namespace

bool parse_cneoidlist_at_fourcc(const hdb::HDBContainer& hdb,
                                  std::size_t fourcc_file_off,
                                  CNeoIDListRecord* out) {
    const auto& raw = hdb.raw_bytes();
    // Record starts 16 bytes BEFORE the fourcc, ends 14 bytes AFTER its start.
    if (fourcc_file_off < 16) return false;
    if (fourcc_file_off + 14 > raw.size()) return false;
    const uint8_t* base = raw.data() + (fourcc_file_off - 16);

    uint32_t flag    = read_be32(base + 0x00);
    uint32_t count   = read_be32(base + 0x04);
    uint32_t offset  = read_be32(base + 0x08);
    uint32_t classid = read_be32(base + 0x0C);
    uint32_t fourcc  = read_be32(base + 0x10);
    uint32_t value   = read_be32(base + 0x14);

    if (flag    != k_cneoidlist_flag)    return false;
    if (classid != k_cneoidlist_classid) return false;
    if (fourcc  != k_cneoidlist_fourcc)  return false;
    // Validate pad bytes [0x18..0x1D] : ff 00 00 00 00 00
    if (base[0x18] != 0xff) return false;
    for (int k = 0x19; k <= 0x1D; ++k) {
        if (base[k] != 0x00) return false;
    }

    if (out) {
        out->flag_const = flag;
        out->count      = count;
        out->offset     = offset;
        out->class_id   = classid;
        out->fourcc     = fourcc;
        out->value      = value;
        std::memcpy(out->pad, base + 0x18, 6);
        out->file_off   = fourcc_file_off - 16;
    }
    return true;
}

std::vector<uint32_t> resolve_cneoidlist_targets(const hdb::HDBContainer& hdb,
                                                   const CNeoIDListRecord& rec) {
    std::vector<uint32_t> out;
    // Empty lists (count < 2) have no IDs after the marker.
    if (rec.count < 2) return out;
    const auto& raw = hdb.raw_bytes();
    std::size_t end = static_cast<std::size_t>(rec.offset) + static_cast<std::size_t>(rec.count) * 4u;
    if (rec.offset >= raw.size() || end > raw.size()) return out;
    // Validate marker invariant : bytes [+0x02..0x05] == 00 00 00 01
    const uint8_t* p = raw.data() + rec.offset;
    if (p[2] != 0x00 || p[3] != 0x00 || p[4] != 0x00 || p[5] != 0x01) return out;
    // flag16 at bytes [6..7] BE must equal rec.count
    uint16_t flag16 = (uint16_t(p[6]) << 8) | uint16_t(p[7]);
    if (flag16 != rec.count) return out;
    // Skip 8-byte marker, read (count - 2) uint32 IDs (BE)
    std::size_t n_ids = static_cast<std::size_t>(rec.count) - 2u;
    out.reserve(n_ids);
    for (std::size_t k = 0; k < n_ids; ++k) {
        std::size_t off = rec.offset + 8u + k * 4u;
        out.push_back(read_be32(raw.data() + off));
    }
    return out;
}

std::vector<CNeoIDListRecord> scan_cneoidlist_records(const hdb::HDBContainer& hdb) {
    std::vector<CNeoIDListRecord> out;
    const auto& raw = hdb.raw_bytes();
    const uint8_t fcc[4] = {'I', 'D', ' ', ' '};
    // Reserve heuristic : 15606 expected  doc.
    out.reserve(16 * 1024);
    for (std::size_t i = 0; i + 4 <= raw.size(); ++i) {
        if (std::memcmp(&raw[i], fcc, 4) != 0) continue;
        CNeoIDListRecord r;
        if (parse_cneoidlist_at_fourcc(hdb, i, &r)) {
            out.push_back(r);
        }
    }
    return out;
}

}  // namespace nl
}  // namespace xfiles
