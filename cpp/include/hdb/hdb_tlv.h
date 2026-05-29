// SPDX-License-Identifier: MIT
// HDB TLV (Tag-Length-Value) reader/writer.
//
// Each record in an HDB page contains a sequence of TLV entries :
//   tag (1 byte) | sub_tag (1 byte) | flag (2 bytes) | value (variable)
//
// For save classes, value is typically uint32 (4 bytes). For sub-collections,
// value is a list of IDs (4 bytes each) referencing master HDB items.
//
// Reference :
//   the format notes — per-class TLV grammar
//   On-disk format (NeoPersist 3.0).
//   scripts/gam_field_aware_roundtrip.py — Python field-aware parser

#ifndef HDB_TLV_H
#define HDB_TLV_H

#include <cstdint>
#include <cstddef>

namespace xfiles {
namespace hdb {

/// TLV record header (4 bytes : tag + sub_tag + flag16).
struct TLVHeader {
    uint8_t  tag;
    uint8_t  sub_tag;
    uint16_t flag16;
};

static_assert(sizeof(TLVHeader) == 4, "TLVHeader must be 4 bytes");

/// TLV reader — reads tagged fields from a byte stream.
///
/// Usage in VC class Read() :
///   TLVReader r(page_bytes, page_size);
///   uint32_t f_e = r.read_uint32_field(0x65);  // tag 'e'
///   uint32_t f_f = r.read_uint32_field(0x66);  // tag 'f'
class TLVReader {
public:
    TLVReader(const uint8_t* data, std::size_t size) : data_(data), size_(size), pos_(0) {}

    /// Read a uint32 field at the current cursor.
    /// Returns the value at offset+4 within the 8-byte record (tag + sub_tag + flag16 + value32).
    /// Advances cursor by 8 bytes.
    uint32_t read_uint32_field(uint8_t expected_tag);

    /// Read raw bytes.
    void read_bytes(uint8_t* dst, std::size_t n);

    /// Cursor position.
    std::size_t pos() const { return pos_; }
    void seek(std::size_t p) { pos_ = p; }

    /// Remaining bytes.
    std::size_t remaining() const { return size_ - pos_; }

private:
    const uint8_t* data_;
    std::size_t size_;
    std::size_t pos_;
};

/// TLV writer — writes tagged fields to a byte stream.
///
/// Usage in VC class Write() :
///   TLVWriter w(buf, buf_size);
///   w.write_uint32_field(0x65, field_e);
///   w.write_uint32_field(0x66, field_f);
class TLVWriter {
public:
    TLVWriter(uint8_t* data, std::size_t size) : data_(data), size_(size), pos_(0) {}

    /// Write a uint32 field : (tag, sub_tag=0, flag16=0, value32).
    /// Advances cursor by 8 bytes.
    void write_uint32_field(uint8_t tag, uint32_t value);

    /// Write a uint32 field with explicit sub_tag + flag16 (for re-emitting existing TLVs).
    void write_uint32_field_full(uint8_t tag, uint8_t sub_tag, uint16_t flag16, uint32_t value);

    /// Write raw bytes.
    void write_bytes(const uint8_t* src, std::size_t n);

    std::size_t pos() const { return pos_; }

private:
    uint8_t* data_;
    std::size_t size_;
    std::size_t pos_;
};

}  // namespace hdb
}  // namespace xfiles

#endif  // HDB_TLV_H
