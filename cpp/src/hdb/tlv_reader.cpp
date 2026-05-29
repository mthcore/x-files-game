// SPDX-License-Identifier: MIT
// HDB TLV stream reader implementation — minimal byte-direct interpreter
// for the on-disk wire format used by VC class Read methods.

#include "hdb/tlv_reader.h"

#include <cstring>

namespace xfiles {
namespace hdb {

namespace {

// big-endian readers
uint16_t be16(const uint8_t* p) {
    return static_cast<uint16_t>((uint16_t(p[0]) << 8) | uint16_t(p[1]));
}
uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
         | (uint32_t(p[2]) << 8)  |  uint32_t(p[3]);
}

}  // anon

bool MemoryTLVReader::seek_tag(uint8_t tag) {
    // Scan forward for a (tag, len_be16, payload) triple. The on-disk format
    // is dense — tags appear in order written, no padding. We walk byte by
    // byte (one-pass) advancing cursor as we go, until we find the requested
    // tag or fall off the end.
    while (cursor_ + 3 <= size_) {
        uint8_t t = data_[cursor_];
        uint16_t len = be16(data_ + cursor_ + 1);
        if (t == tag) {
            // Leave cursor at start of payload (skipping tag + len bytes).
            cursor_ += 3;
            return true;
        }
        // Advance past this entry's tag + length + payload.
        cursor_ += 3 + len;
        if (cursor_ > size_) break;
    }
    miss_ = true;
    return false;
}

bool MemoryTLVReader::seek_fourcc(uint32_t fourcc) {
    // FourCC tags use the 4-byte tag + payload format (no length prefix —
    // the fourcc itself denotes a known section). We linear-scan for the
    // pattern.
    if (size_ < 4) { miss_ = true; return false; }
    for (std::size_t i = cursor_; i + 4 <= size_; ++i) {
        if (be32(data_ + i) == fourcc) {
            cursor_ = i + 4;
            return true;
        }
    }
    miss_ = true;
    return false;
}

uint32_t MemoryTLVReader::read_uint32(uint8_t tag) {
    std::size_t save = cursor_;
    if (!seek_tag(tag)) { cursor_ = save; return 0; }
    if (cursor_ + 4 > size_) { miss_ = true; cursor_ = save; return 0; }
    uint32_t v = be32(data_ + cursor_);
    cursor_ += 4;
    return v;
}

uint16_t MemoryTLVReader::read_uint16(uint8_t tag) {
    std::size_t save = cursor_;
    if (!seek_tag(tag)) { cursor_ = save; return 0; }
    if (cursor_ + 2 > size_) { miss_ = true; cursor_ = save; return 0; }
    uint16_t v = be16(data_ + cursor_);
    cursor_ += 2;
    return v;
}

void MemoryTLVReader::read_sub_object(uint8_t* /*this_field*/, uint8_t tag) {
    // Sub-objects use the same tag-block convention but their payload is
    // an entire serialized child record. For now we just seek past the tag
    // so subsequent reads continue at the right place. Concrete deserialize
    // is done by the calling VC class (which recursively invokes Read on
    // a fresh sub-TLVReader scoped to the sub-payload).
    std::size_t save = cursor_;
    if (!seek_tag(tag)) { cursor_ = save; return; }
    if (cursor_ + 0 > size_) { miss_ = true; cursor_ = save; return; }
    // The sub-object payload starts at cursor_ ; caller would normally
    // construct a nested MemoryTLVReader over `data_+cursor_, payload_len`
    // and pass it to the child Read method. Here we simply advance.
    // TODO : when callers exist, expose a way to fork a child reader.
}

void MemoryTLVReader::read_tag_block(uint8_t* out, std::size_t out_size,
                                      uint32_t fourcc) {
    std::size_t save = cursor_;
    if (!seek_fourcc(fourcc)) { cursor_ = save; return; }
    std::size_t avail = (cursor_ + out_size <= size_)
        ? out_size
        : (size_ - cursor_);
    if (avail > 0 && out) std::memcpy(out, data_ + cursor_, avail);
    cursor_ += avail;
}

uint32_t MemoryTLVReader::read_uint32_fourcc(uint32_t fourcc) {
    std::size_t save = cursor_;
    if (!seek_fourcc(fourcc)) { cursor_ = save; return 0; }
    if (cursor_ + 4 > size_) { miss_ = true; cursor_ = save; return 0; }
    uint32_t v = be32(data_ + cursor_);
    cursor_ += 4;
    return v;
}

std::string MemoryTLVReader::read_string(uint8_t tag) {
    std::size_t save = cursor_;
    if (!seek_tag(tag)) { cursor_ = save; return {}; }
    // Look back 2 bytes to read the length (we already advanced past it).
    if (cursor_ < 3) return {};
    uint16_t len = be16(data_ + cursor_ - 2);
    if (cursor_ + len > size_) { miss_ = true; cursor_ = save; return {}; }
    // The payload may be NUL-terminated ; trim trailing NUL if present.
    std::size_t real_len = len;
    while (real_len > 0 && data_[cursor_ + real_len - 1] == 0) --real_len;
    std::string s(reinterpret_cast<const char*>(data_ + cursor_), real_len);
    cursor_ += len;
    return s;
}

void MemoryTLVReader::prelude() {
    // Skip the 'NPfl' + 'vers' headers, leaving cursor at the first
    // class-specific tag.
    std::size_t save = cursor_;
    if (!seek_fourcc(kFourcc_NPfl)) { cursor_ = save; return; }
    // After 'NPfl' the format spec describes a fixed-size block ; for now we
    // just skip ahead a few bytes (per the on-disk the call site reads a
    // 2-byte param and a fourcc). The actual offset is data-dependent so
    // miss_ is acceptable if read_uint32/16 calls later don't find their tags.
    // 'vers' is read on demand by callers that need it ; otherwise we leave
    // it for downstream seek_tag to land on the right place.
}

}  // namespace hdb
}  // namespace xfiles
