// SPDX-License-Identifier: MIT
// HDB record index : byte-direct walk of all CNeoPersist records in HDB.
//
// For each record, indexes :
//   - byte_offset (absolute, in the raw HDB bytes)
//   - class_id (from the 8-byte CNeoPersist marker)
//   - payload_size (computed from page slot table)
//
// Provides :
//   - find_by_offset(off) → RecordRef
//   - find_by_class(class_id) → vector<RecordRef>
//   - list_all() → const vector<RecordRef>&
//
// Built once at load time by scanning all 256-byte pages of the HDB.

#ifndef NL_HDB_RECORD_INDEX_H
#define NL_HDB_RECORD_INDEX_H

#include "nl/neo_stream.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace xfiles {
namespace nl {

/// Reference to a record in HDB (does not own the bytes).
struct RecordRef {
    std::size_t byte_offset;     // absolute offset in HDB
    std::size_t payload_size;    // size of payload after the 8-byte marker
    uint16_t    class_id;        // from CNeoPersist header
    uint16_t    packed_flags;
    uint16_t    page_id;
};

/// Walks all pages of an HDB blob, indexes every CNeoPersist record.
class HDBRecordIndex {
public:
    /// Build the index by scanning the HDB byte buffer.
    HDBRecordIndex(const uint8_t* hdb_data, std::size_t hdb_size)
        : data_(hdb_data), size_(hdb_size) {
        build();
    }

    /// Number of records found.
    std::size_t size() const { return records_.size(); }

    /// All records.
    const std::vector<RecordRef>& all() const { return records_; }

    /// Find a record by its absolute byte offset.
    const RecordRef* find_by_offset(std::size_t off) const {
        auto it = by_offset_.find(off);
        if (it == by_offset_.end()) return nullptr;
        return &records_[it->second];
    }

    /// Find all records of a given class.
    std::vector<RecordRef> find_by_class(uint16_t class_id) const {
        std::vector<RecordRef> out;
        for (const auto& r : records_) {
            if (r.class_id == class_id) out.push_back(r);
        }
        return out;
    }

    /// Get a record's payload bytes (post-marker).
    /// Returns null if the offset is invalid.
    const uint8_t* payload_at(std::size_t off) const {
        if (off + 8 > size_) return nullptr;
        return data_ + off + 8;
    }

    /// Raw HDB data pointer (for byte-direct ops outside the index).
    const uint8_t* hdb_data() const { return data_; }
    std::size_t    hdb_size() const { return size_; }

    /// Quick class id lookup from raw offset (skips index).
    uint16_t class_id_at(std::size_t off) const {
        if (off + 8 > size_) return 0;
        if (!NeoStream::is_marker(data_, size_, off)) return 0;
        return (static_cast<uint16_t>(data_[off + 6]) << 8) | data_[off + 7];
    }

private:
    const uint8_t* data_;
    std::size_t    size_;
    std::vector<RecordRef> records_;
    std::unordered_map<std::size_t, std::size_t> by_offset_;

    void build() {
        // X-Files HDB layout : 32-byte file header, then 256-byte pages
        std::size_t page_count = (size_ - kHdbFileHeaderSize) / kHdbPageSize;
        records_.reserve(page_count);

        for (std::size_t pi = 0; pi < page_count; ++pi) {
            std::size_t page_off = kHdbFileHeaderSize + pi * kHdbPageSize;
            if (page_off >= size_) break;
            const uint8_t* page = data_ + page_off;
            if (page[0] < 0xC0 || page[0] > 0xCF) continue;

            // Parse 8-byte aligned markers in this page
            std::vector<std::size_t> marker_offs;
            for (std::size_t off = 0; off + 8 <= kHdbPageSize; off += 8) {
                if (page[off] < 0xC0 || page[off] > 0xCF) continue;
                if (page[off + 2] != 0 || page[off + 3] != 0
                    || page[off + 4] != 0 || page[off + 5] != 1) continue;
                marker_offs.push_back(off);
            }

            for (std::size_t i = 0; i < marker_offs.size(); ++i) {
                std::size_t off = marker_offs[i];
                std::size_t next_off = (i + 1 < marker_offs.size())
                                     ? marker_offs[i + 1]
                                     : kHdbPageSize;
                RecordRef r;
                r.byte_offset  = page_off + off;
                r.payload_size = next_off - off - 8;
                r.packed_flags = (static_cast<uint16_t>(page[off]) << 8)
                                | page[off + 1];
                r.class_id     = (static_cast<uint16_t>(page[off + 6]) << 8)
                                | page[off + 7];
                r.page_id      = static_cast<uint16_t>(pi);

                by_offset_[r.byte_offset] = records_.size();
                records_.push_back(r);
            }
        }
    }
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_HDB_RECORD_INDEX_H
