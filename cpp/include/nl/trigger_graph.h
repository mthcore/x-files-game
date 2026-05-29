// SPDX-License-Identifier: MIT
// Trigger graph : decoded VCTrigger record with all sub-records resolved.
//
// Format layout :
//   - Each VCTrigger contains a payload of 24 bytes header + N pairs of
//   - Each pair references a sub-record (Condition/Action/Target/etc.)
//   - 100% of valid pairs resolve byte-direct via HDBRecordIndex
//
// Sub-record classes observed (target_class of pairs):
//   0x10 HBStringRef, 0x11 HBSubScope, 0x14 HBExpression (CONDITION),
//   0x15 HBAssetRef, 0x1B HBActionItem, 0x1C HBStringTag,
//   0x1D HBActionBody (ACTION), 0x33 VCNav, 0x34 VCNavList, 0x51 VCTrigger
//   (nested), 0x52 VCTriggerList, etc.

#ifndef NL_TRIGGER_GRAPH_H
#define NL_TRIGGER_GRAPH_H

#include "nl/hdb_record_index.h"
#include "nl/neo_stream.h"

#include <cstdint>
#include <vector>

namespace xfiles {
namespace nl {

/// A resolved pair from a VCTrigger payload (byte_offset, neoid + target).
struct TriggerPair {
    std::size_t byte_offset;     // byte_offset stored in payload
    uint32_t    neoid;            // neoid stored in payload
    uint16_t    target_class_id;  // class of the resolved record (0 = unresolved)
    std::size_t target_payload_size;
    const uint8_t* target_payload;  // pointer into HDB bytes
};

/// A fully decoded VCTrigger record.
struct DecodedTrigger {
    std::size_t              marker_offset;
    uint16_t                 page_id;
    uint32_t                 trigger_id;
    uint32_t                 field2;
    uint32_t                 field3;
    std::size_t              payload_size;
    std::vector<TriggerPair> pairs;
};

/// Resolve a (byte_offset) hint to a CNeoPersist marker by raw magic check.
/// Mirrors Python L7's resolve_byte_offset : doesn't use the index (which
/// only contains records inside 0xC0-0xCF pages); checks magic anywhere.
inline bool resolve_raw_marker(const uint8_t* data, std::size_t size,
                                std::size_t& boff_inout,
                                uint16_t& class_id_out) {
    auto check = [&](std::size_t off) -> bool {
        if (off + 8 > size) return false;
        return data[off + 2] == 0x00 && data[off + 3] == 0x00
            && data[off + 4] == 0x00 && data[off + 5] == 0x01;
    };
    auto read_cid = [&](std::size_t off) -> uint16_t {
        return (static_cast<uint16_t>(data[off + 6]) << 8) | data[off + 7];
    };

    if (boff_inout < 32 || boff_inout + 8 > size) return false;
    if (check(boff_inout)) {
        class_id_out = read_cid(boff_inout);
        return true;
    }
    // Delta adjustment for alignment slippage
    constexpr int kDeltas[] = {-4, -8, 4, 8, -16, 16};
    for (int d : kDeltas) {
        std::size_t adj = static_cast<std::size_t>(
            static_cast<int64_t>(boff_inout) + d);
        if (adj < 32 || adj + 8 > size) continue;
        if (check(adj)) {
            boff_inout = adj;
            class_id_out = read_cid(adj);
            return true;
        }
    }
    return false;
}

/// Decode a single VCTrigger record at the given offset.
inline DecodedTrigger decode_trigger(const HDBRecordIndex& index,
                                      std::size_t marker_offset) {
    DecodedTrigger out{};
    out.marker_offset = marker_offset;

    const RecordRef* ref = index.find_by_offset(marker_offset);
    if (!ref || ref->class_id != kClassVCTrigger) return out;

    out.page_id      = ref->page_id;
    out.payload_size = ref->payload_size;

    if (ref->payload_size < 24) return out;

    const uint8_t* payload = index.payload_at(marker_offset);
    if (!payload) return out;
    const uint8_t* hdb_start = index.hdb_data();
    const std::size_t hdb_size = index.hdb_size();

    NeoStream ps(payload, ref->payload_size);
    out.trigger_id = ps.read_long();
    out.field2     = ps.read_long();
    out.field3     = ps.read_long();

    // Skip to pairs region (+0x18 = 24 bytes from payload start)
    ps.set_mark(0x18);

    while (ps.mark() + 8 <= ref->payload_size) {
        uint32_t boff = ps.read_long();
        uint32_t nid  = ps.read_long();
        if (boff < 100 || boff > 6'100'000) continue;
        if (nid == 0 || nid > 1'000'000) continue;

        TriggerPair tp{};
        tp.byte_offset = boff;
        tp.neoid       = nid;

        // Raw magic-byte check (mirrors Python L7 — no page filter)
        std::size_t adj_off = boff;
        uint16_t adj_class = 0;
        if (resolve_raw_marker(hdb_start, hdb_size, adj_off, adj_class)) {
            tp.byte_offset     = adj_off;
            tp.target_class_id = adj_class;

            // Always expose payload pointer (= mark + 8).
            tp.target_payload = hdb_start + adj_off + 8;

            // For records in 0xC0-0xCF pages, use index's exact size.
            const RecordRef* indexed = index.find_by_offset(adj_off);
            if (indexed) {
                tp.target_payload_size = indexed->payload_size;
            } else {
                // For records outside indexed pages, scan for next marker
                // on 8-byte alignment (matches the on-disk slot grid).
                // Use 8-byte aligned search starting at adj_off + 8.
                std::size_t end = adj_off + 8;
                std::size_t scan_limit = (adj_off + 256 < hdb_size)
                                       ? adj_off + 256 : hdb_size;
                while (end + 8 <= scan_limit) {
                    // Only consider 8-byte aligned positions
                    if ((end - adj_off) % 8 == 0
                        && end + 6 <= hdb_size
                        && hdb_start[end + 2] == 0x00
                        && hdb_start[end + 3] == 0x00
                        && hdb_start[end + 4] == 0x00
                        && hdb_start[end + 5] == 0x01) {
                        // Verify it's a plausible marker : class_id in
                        // 0x00-0x5C range
                        uint16_t cid = (static_cast<uint16_t>(hdb_start[end + 6]) << 8)
                                     | hdb_start[end + 7];
                        if (cid <= 0x5C) break;
                    }
                    end += 8;
                }
                tp.target_payload_size = (end > adj_off + 8)
                                       ? end - adj_off - 8 : 8;
            }
        }
        out.pairs.push_back(tp);
    }
    return out;
}

/// Decode all VCTriggers in HDB.
inline std::vector<DecodedTrigger> decode_all_triggers(
    const HDBRecordIndex& index) {
    std::vector<DecodedTrigger> out;
    auto triggers = index.find_by_class(kClassVCTrigger);
    out.reserve(triggers.size());
    for (const auto& r : triggers) {
        out.push_back(decode_trigger(index, r.byte_offset));
    }
    return out;
}

}  // namespace nl
}  // namespace xfiles

#endif  // NL_TRIGGER_GRAPH_H
