// SPDX-License-Identifier: MIT
// NL leaf-record walker — extracts variable-size records from a 0xC3 leaf page.
//
// Empirical decoding (2026-05-15) based on observation of 202 leaves in
// XFILES.HDB :
//
// Every leaf record starts with an 8-byte marker invariant :
//
//   cX YY 00 00 00 01 ?? ??
//   ^^ ^^ ^^^^^^^^^^^ ^^^^^
//   |  |  |           |
//   |  |  |           +--- flag16 (semantics TBD ; often 0x0000)
//   |  |  +--------------- universal "01" marker (bytes [2..5] = 00 00 00 01)
//   |  +--------------------- per-record sub-tag (class hint / first-ID-byte)
//   +------------------------- marker family : 0xc0..0xcf
//
// The record's payload follows the marker and continues until the NEXT
// marker (or end of page). Payload bytes are typed 8-byte sub-records :
//
//   tag (u8) | sub_tag (u8) | flag16 (BE u16) | value32 (BE u32)
//
// which can be decoded via `hdb::TLVReader` / `hdb_parse_page`.
//
// This is the byte-direct iso of the leaf-walking portion of the on-disk format's
// the on-disk reader -> `items_table->vtable[+0x78]` chain. The full lookup
// chain (B-tree descent through 0xC2 internals) is still TODO.

#ifndef NL_LEAF_WALKER_H
#define NL_LEAF_WALKER_H

#include "hdb/hdb_container.h"

#include <cstdint>
#include <vector>

namespace xfiles {
namespace nl {

/// A walked leaf record. Holds the marker bytes + the byte range of the
/// payload following it (sub-records can be parsed via hdb_parse_page on
/// the slice `[payload_off, payload_off + payload_size)`).
struct LeafRecord {
    uint8_t  marker_tag;     ///< byte 0 of marker (cX where X in 0..f)
    uint8_t  marker_sub_tag; ///< byte 1 of marker (per-record class hint)
    uint16_t marker_flag16;  ///< bytes [6..7] of marker (BE u16, semantics TBD)
    std::size_t marker_off;  ///< offset of marker bytes in the page (0..255)
    std::size_t payload_off; ///< marker_off + 8
    std::size_t payload_size; ///< bytes from payload_off to next marker / end
};

/// Walk a 0xC3 leaf page and return all records found within it.
/// Returns an empty vector if `page.bytes[0]` is not 0xC3.
///
/// Implementation rule : a marker is any 8-byte sequence at an 8-aligned
/// offset whose bytes [2..5] == { 00, 00, 00, 01 } AND whose byte [0] is in
/// the range 0xC0..0xCF.
std::vector<LeafRecord> walk_leaf_records(const hdb::Page& page);

/// Walk a 0xC2 internal page (B-tree internal node). Same marker invariant
/// as leaves. Returns an empty vector if `page.bytes[0]` is not 0xC2.
///
/// Internal pages hold a mix of key markers (`c2 sub`) + child pointer
/// markers (`c3 sub` referencing leaves OR `c2 sub` referencing other
/// internals). The exact field semantics within each record are not yet
/// decoded, but the same `walk_*_records` extraction works because the
/// marker invariant `(byte0 in 0xC0..0xCF, bytes[2..5] == 00 00 00 01)` is
/// universal across both internal and leaf pages (empirically verified).
std::vector<LeafRecord> walk_internal_records(const hdb::Page& page);

/// Generic walker — applies to any HDB page whose first byte is in
/// 0xC0..0xCF (covers internal 0xC2, leaf 0xC3, alt-internal 0xD2 ; the
/// 0xD2 family also follows the same invariant per spot checks).
std::vector<LeafRecord> walk_btree_records(const hdb::Page& page);

/// Apply `walk_leaf_records` to every 0xC3 leaf page of an HDB container.
/// Yields one flat vector with `(page_idx, LeafRecord)` pairs. Useful for
/// statistics + global lookups.
struct GlobalLeafRecord {
    std::size_t page_idx;
    LeafRecord  rec;
};
std::vector<GlobalLeafRecord> walk_all_leaves(const hdb::HDBContainer& hdb);

/// Extract the payload bytes of a leaf record into `dest`. Returns the
/// number of bytes copied (== `rec.payload_size`, capped by `dest_cap`).
/// `dest` may be nullptr to get the size without copying.
std::size_t extract_leaf_payload(const hdb::Page& page,
                                  const LeafRecord& rec,
                                  uint8_t* dest, std::size_t dest_cap);

/// Filter records walked from all leaves by `(marker_tag, marker_sub_tag)`.
/// Returns only those records matching both. Either value can be 0xFF as a
/// wildcard.
std::vector<GlobalLeafRecord> filter_leaves(const hdb::HDBContainer& hdb,
                                             uint8_t marker_tag,
                                             uint8_t marker_sub_tag);

/// Parse the payload bytes of a leaf record as a sequence of 8-byte typed
/// sub-records (universal Record8 format : tag, sub_tag, flag16, value32).
///
/// Each VC* Read method walks the wire stream looking for sub-records of
/// specific tags ; this function exposes the same sequence so per-class
/// decoders can match `(tag_byte) -> field` lookups against real HDB bytes.
///
/// `payload_size % 8 != 0` payloads are truncated to the largest multiple
/// of 8 (we drop trailing partial bytes).
std::vector<hdb::Record8> parse_leaf_sub_records(const hdb::Page& page,
                                                   const LeafRecord& rec);

/// Find the first sub-record matching `tag_byte` in the payload of `rec`.
/// Returns true (filling `*out`) if a match exists, false otherwise.
///
/// Mirrors the on-disk format's `read_uint32_by_tag(tag)` vtable[+0x9c] call from
/// etc.
bool find_sub_record_by_tag(const hdb::Page& page,
                             const LeafRecord& rec,
                             uint8_t tag_byte,
                             hdb::Record8* out);

}  // namespace nl
}  // namespace xfiles

#endif  // NL_LEAF_WALKER_H
