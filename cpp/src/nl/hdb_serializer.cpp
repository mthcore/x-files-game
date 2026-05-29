// SPDX-License-Identifier: MIT
// In-memory HDBSerializer implementation.

#include "nl/hdb_serializer.h"

#include <cstring>

namespace xfiles {
namespace nl {

InMemoryHDBSerializer::InMemoryHDBSerializer(const uint8_t* data,
                                             std::size_t size)
    : data_(data), size_(size), cursor_(0), error_(false) {}

void InMemoryHDBSerializer::reset() {
    cursor_ = 0;
    error_  = false;
}

bool InMemoryHDBSerializer::ensure_remaining(std::size_t n) {
    if (error_) return false;
    if (cursor_ + n > size_) {
        error_ = true;
        return false;
    }
    return true;
}

bool InMemoryHDBSerializer::expect_byte_tag(uint8_t expected) {
    if (!ensure_remaining(1)) return false;
    uint8_t got = data_[cursor_++];
    if (got != expected) {
        error_ = true;
        return false;
    }
    return true;
}

bool InMemoryHDBSerializer::expect_fourcc_tag(uint32_t expected) {
    if (!ensure_remaining(4)) return false;
    // Fourcc is stored as 4 ASCII bytes in stream order : 'NPfl' →
    // bytes 0x4e 0x50 0x66 0x6c. We assemble big-endian to match the
    // numeric constants used by the format (0x4e50666c).
    uint32_t got = (static_cast<uint32_t>(data_[cursor_])     << 24)
                 | (static_cast<uint32_t>(data_[cursor_ + 1]) << 16)
                 | (static_cast<uint32_t>(data_[cursor_ + 2]) << 8)
                 |  static_cast<uint32_t>(data_[cursor_ + 3]);
    cursor_ += 4;
    if (got != expected) {
        error_ = true;
        return false;
    }
    return true;
}

uint16_t InMemoryHDBSerializer::read_u16_le_raw() {
    uint16_t v = static_cast<uint16_t>(data_[cursor_])
               | (static_cast<uint16_t>(data_[cursor_ + 1]) << 8);
    cursor_ += 2;
    return v;
}

uint32_t InMemoryHDBSerializer::read_u32_le_raw() {
    uint32_t v = static_cast<uint32_t>(data_[cursor_])
               | (static_cast<uint32_t>(data_[cursor_ + 1]) << 8)
               | (static_cast<uint32_t>(data_[cursor_ + 2]) << 16)
               | (static_cast<uint32_t>(data_[cursor_ + 3]) << 24);
    cursor_ += 4;
    return v;
}

int16_t InMemoryHDBSerializer::read_i16_le_raw() {
    return static_cast<int16_t>(read_u16_le_raw());
}

std::string InMemoryHDBSerializer::read_string(uint8_t tag) {
    if (!expect_byte_tag(tag)) return {};
    if (!ensure_remaining(2))  return {};
    uint16_t len = read_u16_le_raw();
    if (!ensure_remaining(len)) return {};
    std::string out(reinterpret_cast<const char*>(data_ + cursor_), len);
    cursor_ += len;
    return out;
}

void InMemoryHDBSerializer::read_buf_fourcc(void* dest, std::size_t count,
                                            uint32_t fourcc) {
    if (!expect_fourcc_tag(fourcc)) return;
    if (!ensure_remaining(count))   return;
    std::memcpy(dest, data_ + cursor_, count);
    cursor_ += count;
}

uint8_t InMemoryHDBSerializer::read_byte_alt(uint8_t tag) {
    return read_byte(tag);
}

uint8_t InMemoryHDBSerializer::read_byte(uint8_t tag) {
    if (!expect_byte_tag(tag)) return 0;
    if (!ensure_remaining(1))  return 0;
    return data_[cursor_++];
}

uint32_t InMemoryHDBSerializer::read_dword(uint8_t tag) {
    if (!expect_byte_tag(tag)) return 0;
    if (!ensure_remaining(4))  return 0;
    return read_u32_le_raw();
}

void InMemoryHDBSerializer::begin_record() {
    // In the wire format used by the in-memory serializer, no session
    // header precedes the first field. The on-disk format uses this slot
    // to advance past per-record framing ; we keep it as a hookpoint.
}

int32_t InMemoryHDBSerializer::read_short(uint8_t tag) {
    if (!expect_byte_tag(tag)) return 0;
    if (!ensure_remaining(2))  return 0;
    return static_cast<int32_t>(read_i16_le_raw());
}

uint32_t InMemoryHDBSerializer::read_dword_fourcc(uint32_t fourcc) {
    if (!expect_fourcc_tag(fourcc)) return 0;
    if (!ensure_remaining(4))       return 0;
    return read_u32_le_raw();
}

// ---------------------------------------------------------------------------
// Write side : append to output_ buffer matching the read_* wire format.
// ---------------------------------------------------------------------------

namespace {

void emit_u8(std::vector<uint8_t>& out, uint8_t v) {
    out.push_back(v);
}

void emit_u16_le(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
}

void emit_u32_le(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>( v        & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 8)  & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
}

void emit_fourcc(std::vector<uint8_t>& out, uint32_t fc) {
    out.push_back(static_cast<uint8_t>((fc >> 24) & 0xff));
    out.push_back(static_cast<uint8_t>((fc >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((fc >> 8)  & 0xff));
    out.push_back(static_cast<uint8_t>( fc        & 0xff));
}

}  // anon

void InMemoryHDBSerializer::write_buf_fourcc(const void* src,
                                              std::size_t count,
                                              uint32_t fourcc) {
    emit_fourcc(output_, fourcc);
    const uint8_t* p = static_cast<const uint8_t*>(src);
    for (std::size_t i = 0; i < count; ++i) emit_u8(output_, p[i]);
}

void InMemoryHDBSerializer::write_byte(uint8_t tag, uint8_t v) {
    emit_u8(output_, tag);
    emit_u8(output_, v);
}

void InMemoryHDBSerializer::write_short(uint8_t tag, int32_t v) {
    emit_u8(output_, tag);
    emit_u16_le(output_, static_cast<uint16_t>(v));
}

void InMemoryHDBSerializer::write_dword(uint8_t tag, uint32_t v) {
    emit_u8(output_, tag);
    emit_u32_le(output_, v);
}

void InMemoryHDBSerializer::write_dword_fourcc(uint32_t fourcc, uint32_t v) {
    emit_fourcc(output_, fourcc);
    emit_u32_le(output_, v);
}

void InMemoryHDBSerializer::write_string(uint8_t tag, const std::string& s) {
    emit_u8(output_, tag);
    emit_u16_le(output_, static_cast<uint16_t>(s.size()));
    for (char c : s) emit_u8(output_, static_cast<uint8_t>(c));
}

}  // namespace nl
}  // namespace xfiles
