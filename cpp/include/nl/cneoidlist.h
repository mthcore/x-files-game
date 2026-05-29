// SPDX-License-Identifier: MIT
// CNeoIDList - byte-direct decoder for the simplest NL persistent class.
//
// On-disk layout (30 bytes, big-endian, found verbatim in HDB) :
//
//   +0x00..03 : flag_const = 
//   +0x04..07 : count       (number of IDs in this list, BE u32)
//   +0x08..0B : offset      (file offset to associated record data, BE u32)
//   +0x0C..0F : class_id    =  (CNeoIDList)
//   +0x10..13 : fourcc      = 'ID  '  (0x49 0x44 0x20 0x20)
//   +0x14..17 : value       (BE u32, primary ID this list anchors)
//   +0x18..1D : pad         = 0xff 0x00 0x00 0x00 0x00 0x00 (constant)
//
// This is the SIMPLEST NL persistent class to decode because the 'ID  '
// fourcc is stored verbatim ( doc, the only fourcc not remapped by
// the serializer). All 15606 occurrences in XFILES.HDB follow this exact
// layout (empirically verified, 2026-05-15 cycle 9).
//
// Field semantics (analyzed cycle 10) :
//   - `flag_const = ` : always-1 constant (likely refcount or
//     "alive" marker, not a sequential ID)
//   - `count`     : number of IDs in the linked target record (1 majority,
//     1..21+ observed)
//   - `offset`    : file offset to ANOTHER record that starts with a leaf
//     marker `cX YY 00 00 00 01 NN NN` where flag16 NN NN = count. So
//     CNeoIDList is a POINTER to an ID list, not the list itself.
//   - `class_id  = ` : CNeoIDList type tag
//   - `fourcc   = 'ID  '`     : verbatim fourcc marker (W14)
//   - `value    = ` : **ALWAYS 1** on all 15426 decoded records.
//     This is a constant, NOT the persistent ID. The actual persistent ID
//     of the list is reachable via the linked record at `offset`.
//   - `pad     = ff 00 00 00 00 00` : constant trailer

#ifndef NL_CNEOIDLIST_H
#define NL_CNEOIDLIST_H

#include "hdb/hdb_container.h"

#include <cstdint>
#include <vector>

namespace xfiles {
namespace nl {

/// Decoded CNeoIDList record (30 bytes on disk).
struct CNeoIDListRecord {
    uint32_t flag_const;    ///< should be 
    uint32_t count;         ///< number of IDs in this list
    uint32_t offset;        ///< target file offset (often points to record data)
    uint32_t class_id;      ///< should be 0x0b (CNeoIDList)
    uint32_t fourcc;        ///< should be 0x49442020 ('ID  ')
    uint32_t value;         ///< primary ID this list anchors
    uint8_t  pad[6];        ///< should be { 0xff, 0, 0, 0, 0, 0 }
    std::size_t file_off;   ///< file offset where this record starts
};

/// Constants for validation.
constexpr uint32_t k_cneoidlist_flag    = 0x4E50666C;  // 'NPfl'
constexpr uint32_t k_cneoidlist_classid = 0x0000000B;  // CNeoIDList class_id 0x0b
constexpr uint32_t k_cneoidlist_fourcc  = 0x49442020;  // 'ID  '

/// Scan an HDB container for every CNeoIDList record (i.e., every 'ID  '
/// fourcc occurrence preceded by the 12-byte header). Returns one entry
/// per record.
///
/// Records that don't match the full layout (flag != 1, class_id != 0x0b,
/// pad bytes off) are skipped to avoid false positives.
std::vector<CNeoIDListRecord> scan_cneoidlist_records(const hdb::HDBContainer& hdb);

/// Validate that a candidate at `file_off` (offset of the 'ID  ' fourcc
/// inside the file, not the record start) is a well-formed CNeoIDList.
/// Returns true and fills `*out` if so.
bool parse_cneoidlist_at_fourcc(const hdb::HDBContainer& hdb,
                                  std::size_t fourcc_file_off,
                                  CNeoIDListRecord* out);

/// Resolve the target IDs that a CNeoIDList record points to.
///
/// At `rec.offset` lives another record whose layout is :
///
///   +0x00 : 8-byte leaf marker `cX YY 00 00 00 01 NN NN`
///           where flag16 (NN NN, BE u16) == rec.count
///   +0x08 : (rec.count - 2) * 4 bytes of uint32 BE IDs
///   total bytes = rec.count * 4
///
/// So an N-element ID list occupies `(N + 2) * 4` bytes :
/// 8 bytes for the marker + N*4 bytes for the IDs. Empirically observed.
///
/// Returns the extracted IDs (empty if rec.count < 2, or if the marker at
/// `offset` doesn't match the expected invariant).
std::vector<uint32_t> resolve_cneoidlist_targets(const hdb::HDBContainer& hdb,
                                                   const CNeoIDListRecord& rec);

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CNEOIDLIST_H
