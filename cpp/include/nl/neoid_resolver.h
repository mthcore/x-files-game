// SPDX-License-Identifier: MIT
// NeoidResolver — byte-direct neoid → record_offset master table.
//
// X-Files HDB has ~79,775 (byte_offset, neoid) pairs scattered throughout
// the file (especially in CNeoIDList and VCNode payloads). Together they
// form a global resolver : neoid → byte_offset → record marker.
//
// Validated 2026-05-19 :
//   - 68,586 unique neoids resolve byte-direct
//   - VCNode view[0] = neoid 40742 (VCNode 0x28)
//   - VCNode view[1] = neoid 40743 (VCNode 0x28)
//   - ...
//
// This unlocks the navigation pipeline :
//   HOT click action_id
//     → VCNode.action_table.lookup(action_id) = view_neoid (TODO decode)
//     → NeoidResolver.resolve(view_neoid) = byte_offset
//     → byte_offset record contains target file_id (TODO decode view fields)
//     → SceneAtlas.lookup(file_id) → XMV path → CinematicPlayer.load

#ifndef NL_NEOID_RESOLVER_H
#define NL_NEOID_RESOLVER_H

#include "nl/neo_stream.h"

#include <cstdint>
#include <cstring>
#include <unordered_map>

namespace xfiles {
namespace nl {

class NeoidResolver {
public:
    /// Scan HDB byte-direct for all (byte_offset, neoid) pairs.
    /// Each pair is 8 bytes : 4B byte_offset BE + 4B neoid BE.
    /// Valid pair = byte_offset is a marker AND class_id <= 0x5C
    /// AND neoid in 1..100000.
    NeoidResolver(const uint8_t* hdb_data, std::size_t hdb_size) {
        build(hdb_data, hdb_size);
    }

    /// Resolve a neoid to its record byte_offset (= marker offset).
    /// Returns 0 if not found.
    std::size_t resolve(uint32_t neoid) const {
        auto it = neoid_to_offset_.find(neoid);
        return (it == neoid_to_offset_.end()) ? 0u : it->second;
    }

    /// Resolve a neoid to its class_id.
    /// Returns 0xFFFF if not found.
    uint16_t resolve_class(uint32_t neoid) const {
        auto it = neoid_to_class_.find(neoid);
        return (it == neoid_to_class_.end()) ? 0xFFFFu : it->second;
    }

    std::size_t size() const { return neoid_to_offset_.size(); }

private:
    void build(const uint8_t* data, std::size_t size) {
        if (size < 8) return;
        // Reserve based on expected ~70k unique neoids
        neoid_to_offset_.reserve(80000);
        neoid_to_class_.reserve(80000);

        for (std::size_t off = 32; off + 8 <= size; off += 4) {
            // Read 4B byte_offset BE
            uint32_t boff = (uint32_t(data[off])     << 24)
                          | (uint32_t(data[off + 1]) << 16)
                          | (uint32_t(data[off + 2]) << 8)
                          |  uint32_t(data[off + 3]);
            if (boff < 32 || boff + 8 > size) continue;
            // Check it's a valid marker (NPvr magic = 00 00 00 01)
            if (data[boff + 2] != 0x00 || data[boff + 3] != 0x00
                || data[boff + 4] != 0x00 || data[boff + 5] != 0x01) continue;
            uint16_t cid = (uint16_t(data[boff + 6]) << 8) | data[boff + 7];
            if (cid > 0x5C) continue;

            // Read 4B neoid BE
            uint32_t nid = (uint32_t(data[off + 4]) << 24)
                         | (uint32_t(data[off + 5]) << 16)
                         | (uint32_t(data[off + 6]) << 8)
                         |  uint32_t(data[off + 7]);
            if (nid < 1 || nid > 100000) continue;

            // First-wins (matches the on-disk format's iteration order on CNeoIDIndex)
            if (neoid_to_offset_.find(nid) == neoid_to_offset_.end()) {
                neoid_to_offset_[nid] = boff;
                neoid_to_class_[nid] = cid;
            }
        }
    }

    std::unordered_map<uint32_t, std::size_t> neoid_to_offset_;
    std::unordered_map<uint32_t, uint16_t>    neoid_to_class_;
};

/// VCNode views byte-direct decoder.
/// Format layout :
///   Each VCNode record at +0x10 has N consecutive uint32 BE neoids = the
///   "views" / children of that node.
///   Count is at +0x0C low 16 bits.
struct VCNodeViews {
    uint32_t parent_v32;     // +0x00
    uint32_t flags;          // +0x04
    uint32_t second_v32;     // +0x08
    uint32_t count_field;    // +0x0C
    std::vector<uint32_t> view_neoids;

    static VCNodeViews decode(const uint8_t* payload, std::size_t size) {
        VCNodeViews v{};
        if (size < 16) return v;
        v.parent_v32  = (uint32_t(payload[0])  << 24) | (uint32_t(payload[1])  << 16)
                      | (uint32_t(payload[2])  << 8)  |  uint32_t(payload[3]);
        v.flags       = (uint32_t(payload[4])  << 24) | (uint32_t(payload[5])  << 16)
                      | (uint32_t(payload[6])  << 8)  |  uint32_t(payload[7]);
        v.second_v32  = (uint32_t(payload[8])  << 24) | (uint32_t(payload[9])  << 16)
                      | (uint32_t(payload[10]) << 8)  |  uint32_t(payload[11]);
        v.count_field = (uint32_t(payload[12]) << 24) | (uint32_t(payload[13]) << 16)
                      | (uint32_t(payload[14]) << 8)  |  uint32_t(payload[15]);
        uint32_t n = v.count_field & 0xFFFF;
        if (n > 30) return v;  // sanity cap
        v.view_neoids.reserve(n);
        for (uint32_t i = 0; i < n; ++i) {
            std::size_t off = 16 + i * 4;
            if (off + 4 > size) break;
            uint32_t neoid = (uint32_t(payload[off])     << 24)
                           | (uint32_t(payload[off + 1]) << 16)
                           | (uint32_t(payload[off + 2]) << 8)
                           |  uint32_t(payload[off + 3]);
            if (neoid > 0) v.view_neoids.push_back(neoid);
        }
        return v;
    }
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_NEOID_RESOLVER_H
