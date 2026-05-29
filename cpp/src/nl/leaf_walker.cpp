// SPDX-License-Identifier: MIT
// NL leaf-record walker implementation.

#include "nl/leaf_walker.h"

#include <cstring>

namespace xfiles {
namespace nl {

namespace {
// Common walker shared by leaf / internal / btree variants. The only
// difference between callers is whether they pre-filter on a specific
// page tag (0xC2 vs 0xC3 vs accept-any-in-cX-family).
std::vector<LeafRecord> walk_page_records_common(const hdb::Page& page) {
    std::vector<LeafRecord> records;
    // Collect all marker offsets first.
    // Marker rule : byte[0] in 0xc0..0xcf AND bytes[2..5] == 00 00 00 01
    std::vector<std::size_t> marker_offs;
    for (std::size_t off = 0; off + 8 <= 256; off += 8) {
        uint8_t t0 = page.bytes[off];
        if (t0 < 0xC0 || t0 > 0xCF) continue;
        if (page.bytes[off+2] != 0x00) continue;
        if (page.bytes[off+3] != 0x00) continue;
        if (page.bytes[off+4] != 0x00) continue;
        if (page.bytes[off+5] != 0x01) continue;
        marker_offs.push_back(off);
    }
    if (marker_offs.empty()) return records;

    records.reserve(marker_offs.size());
    for (std::size_t k = 0; k < marker_offs.size(); ++k) {
        std::size_t off = marker_offs[k];
        std::size_t next_off = (k + 1 < marker_offs.size())
                                 ? marker_offs[k + 1]
                                 : 256u;
        LeafRecord r{};
        r.marker_tag     = page.bytes[off];
        r.marker_sub_tag = page.bytes[off + 1];
        r.marker_flag16  = static_cast<uint16_t>(
            (uint16_t(page.bytes[off + 6]) << 8) | page.bytes[off + 7]);
        r.marker_off     = off;
        r.payload_off    = off + 8;
        r.payload_size   = (next_off >= off + 8) ? next_off - (off + 8) : 0;
        records.push_back(r);
    }
    return records;
}
}  // namespace

std::vector<LeafRecord> walk_leaf_records(const hdb::Page& page) {
    if (page.bytes[0] != 0xC3) return {};
    return walk_page_records_common(page);
}

std::vector<LeafRecord> walk_internal_records(const hdb::Page& page) {
    if (page.bytes[0] != 0xC2) return {};
    return walk_page_records_common(page);
}

std::vector<LeafRecord> walk_btree_records(const hdb::Page& page) {
    uint8_t t0 = page.bytes[0];
    if (t0 < 0xC0 || t0 > 0xCF) {
        // 0xD2 alt-internal family is also valid (empirically same invariant).
        if (t0 != 0xD2) return {};
    }
    return walk_page_records_common(page);
}

std::vector<GlobalLeafRecord> walk_all_leaves(const hdb::HDBContainer& hdb) {
    std::vector<GlobalLeafRecord> out;
    const std::size_t pages = hdb.page_count();
    for (std::size_t pi = 0; pi < pages; ++pi) {
        const hdb::Page& p = hdb.page(pi);
        if (p.bytes[0] != 0xC3) continue;
        auto recs = walk_leaf_records(p);
        for (const auto& r : recs) {
            out.push_back({pi, r});
        }
    }
    return out;
}

std::size_t extract_leaf_payload(const hdb::Page& page,
                                  const LeafRecord& rec,
                                  uint8_t* dest, std::size_t dest_cap) {
    std::size_t n = rec.payload_size;
    if (n > dest_cap) n = dest_cap;
    if (rec.payload_off + n > 256) {
        n = (rec.payload_off >= 256) ? 0 : 256 - rec.payload_off;
    }
    if (dest != nullptr && n > 0) {
        std::memcpy(dest, page.bytes + rec.payload_off, n);
    }
    return n;
}

std::vector<hdb::Record8> parse_leaf_sub_records(const hdb::Page& page,
                                                   const LeafRecord& rec) {
    std::vector<hdb::Record8> out;
    std::size_t off = rec.payload_off;
    std::size_t end = rec.payload_off + rec.payload_size;
    if (end > 256) end = 256;
    while (off + 8 <= end) {
        out.push_back(hdb::Record8::parse(page.bytes + off));
        off += 8;
    }
    return out;
}

bool find_sub_record_by_tag(const hdb::Page& page,
                             const LeafRecord& rec,
                             uint8_t tag_byte,
                             hdb::Record8* out) {
    std::size_t off = rec.payload_off;
    std::size_t end = rec.payload_off + rec.payload_size;
    if (end > 256) end = 256;
    while (off + 8 <= end) {
        hdb::Record8 r = hdb::Record8::parse(page.bytes + off);
        if (r.tag == tag_byte) {
            if (out) *out = r;
            return true;
        }
        off += 8;
    }
    return false;
}

std::vector<GlobalLeafRecord> filter_leaves(const hdb::HDBContainer& hdb,
                                             uint8_t marker_tag,
                                             uint8_t marker_sub_tag) {
    std::vector<GlobalLeafRecord> out;
    const std::size_t pages = hdb.page_count();
    for (std::size_t pi = 0; pi < pages; ++pi) {
        const hdb::Page& p = hdb.page(pi);
        if (p.bytes[0] != 0xC3) continue;
        auto recs = walk_leaf_records(p);
        for (const auto& r : recs) {
            if (marker_tag != 0xFF && r.marker_tag != marker_tag) continue;
            if (marker_sub_tag != 0xFF && r.marker_sub_tag != marker_sub_tag) continue;
            out.push_back({pi, r});
        }
    }
    return out;
}

}  // namespace nl
}  // namespace xfiles
