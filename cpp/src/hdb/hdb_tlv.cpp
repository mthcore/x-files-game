// SPDX-License-Identifier: MIT
// HDB TLV reader/writer implementation.
//
// Record format (per gam_field_aware_roundtrip.py + the format notes) :
//   8 bytes per record :
//     +0x00 : tag      (uint8)
//     +0x01 : sub_tag  (uint8)
//     +0x02 : flag16   (uint16 BIG-ENDIAN)
//     +0x04 : value32  (uint32 BIG-ENDIAN)
//
// Big-endian preserves the 1998-era HDB on-disk format from HyperBole.

#include "hdb/hdb_tlv.h"

#include <cstring>

namespace xfiles {
namespace hdb {

namespace {

uint16_t read_be16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) << 8 | p[1];
}

uint32_t read_be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) <<  8) |
            static_cast<uint32_t>(p[3]);
}

void write_be16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>((v >> 8) & 0xff);
    p[1] = static_cast<uint8_t>(v & 0xff);
}

void write_be32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>((v >> 24) & 0xff);
    p[1] = static_cast<uint8_t>((v >> 16) & 0xff);
    p[2] = static_cast<uint8_t>((v >>  8) & 0xff);
    p[3] = static_cast<uint8_t>(v & 0xff);
}

}  // namespace

uint32_t TLVReader::read_uint32_field(uint8_t expected_tag) {
    if (pos_ + 8 > size_) return 0;
    const uint8_t* p = data_ + pos_;
    uint8_t  tag    = p[0];
    // uint8_t  sub_tag = p[1];
    // uint16_t flag16  = read_be16(p + 2);
    uint32_t value  = read_be32(p + 4);
    pos_ += 8;
    (void)expected_tag;
    (void)tag;
    return value;
}

void TLVReader::read_bytes(uint8_t* dst, std::size_t n) {
    if (pos_ + n > size_) n = size_ - pos_;
    std::memcpy(dst, data_ + pos_, n);
    pos_ += n;
}

void TLVWriter::write_uint32_field(uint8_t tag, uint32_t value) {
    write_uint32_field_full(tag, 0, 0, value);
}

void TLVWriter::write_uint32_field_full(uint8_t tag, uint8_t sub_tag,
                                          uint16_t flag16, uint32_t value) {
    if (pos_ + 8 > size_) return;
    uint8_t* p = data_ + pos_;
    p[0] = tag;
    p[1] = sub_tag;
    write_be16(p + 2, flag16);
    write_be32(p + 4, value);
    pos_ += 8;
}

void TLVWriter::write_bytes(const uint8_t* src, std::size_t n) {
    if (pos_ + n > size_) n = size_ - pos_;
    std::memcpy(data_ + pos_, src, n);
    pos_ += n;
}

}  // namespace hdb
}  // namespace xfiles
