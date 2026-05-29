// SPDX-License-Identifier: MIT
// HDB TLV stream reader — byte-direct port of the on-disk "Pfl serializer"
// (the polymorphic stream passed to every VC class's Read method).
//
// Every VC class's Read body looks like :
//
//   void VCFoo_Read(SerializerCtx* s, this) {
// NPfl + vers prelude
//     if ((*(s+0x10) >> 4) & 1) ...               // optional version branch
//     (*s->vtable[+0x8])(this+OFF1, s, TAG1);     // sub-object read
//     (*s->vtable[+0x9c])(s, TAG2)                // direct uint32 by tag
//     ...
//
// The serializer carries :
//   - source bytes (record + adjacent string blob)
//   - cursor / state
//   - vtable for primitive readers
//
// Reference :
//   on-disk Read sequence — prelude + per-tag reads
//   format notes          — per-class grammars

#ifndef HDB_TLV_READER_H
#define HDB_TLV_READER_H

#include <cstdint>
#include <cstddef>
#include <string>

namespace xfiles {
namespace hdb {

/// FourCC literal helper.
constexpr uint32_t make_fourcc(char a, char b, char c, char d) {
    return (uint32_t(uint8_t(a)) << 24)
         | (uint32_t(uint8_t(b)) << 16)
         | (uint32_t(uint8_t(c)) <<  8)
         |  uint32_t(uint8_t(d));
}

/// 'NPfl' FourCC — NeoPersistent flag block, first tag of every record.
constexpr uint32_t kFourcc_NPfl = make_fourcc('N','P','f','l');
/// 'vers' FourCC — version dword, second tag.
constexpr uint32_t kFourcc_vers = make_fourcc('v','e','r','s');

/// TLV stream reader interface. Each VC class's Read takes a TLVReader* and
/// pulls fields by tag-byte (single-byte ASCII tags like 'e', 'f', 'g', etc.).
class TLVReader {
public:
    virtual ~TLVReader() = default;

    /// Read a uint32 value associated with the given single-byte tag. Tags
    /// in the on-disk format are typically lowercase ASCII (`'a'..'z'`).
    virtual uint32_t read_uint32(uint8_t tag) = 0;

    /// Read a uint16 value by tag.
    virtual uint16_t read_uint16(uint8_t tag) = 0;

    /// Read a sub-object's bytes at `this_offset` (interpretation depends on
    /// the sub-object's vtable). Mirrors the on-disk format's `serializer.vtable[+0x8]`
    /// dispatch.
    virtual void read_sub_object(uint8_t* this_field, uint8_t tag) = 0;

    /// Read a multi-byte tag block (4-char fourcc identifies which TLV
    /// section to advance past — e.g. 'NPfl' header).
    virtual void read_tag_block(uint8_t* out, std::size_t out_size, uint32_t fourcc) = 0;

    /// Read a uint32 by fourcc (used for 'vers').
    virtual uint32_t read_uint32_fourcc(uint32_t fourcc) = 0;

    /// Read a NUL-terminated string by tag. Returns empty string on miss.
    virtual std::string read_string(uint8_t tag) = 0;

    /// Position the stream past the prelude (NPfl + vers).
    /// Equivalent to base init.
    virtual void prelude() = 0;

    /// True if any tagged read returned a "tag not found" miss since
    /// construction (allows callers to be lenient).
    virtual bool any_miss() const = 0;
};

/// In-memory TLV reader : operates on a sequence of `(tag, length, payload)`
/// triples (the on-disk wire format). Each entry is :
///   byte 0   : tag (ASCII)
///   byte 1-2 : be16 length (payload bytes)
///   bytes... : payload
///
/// Records may be packed without padding. The 4-byte fourcc headers
/// ('NPfl', 'vers') sit at the start of every record.
class MemoryTLVReader : public TLVReader {
public:
    MemoryTLVReader(const uint8_t* data, std::size_t size) noexcept
        : data_(data), size_(size), cursor_(0), miss_(false) {}

    uint32_t read_uint32(uint8_t tag)      override;
    uint16_t read_uint16(uint8_t tag)      override;
    void     read_sub_object(uint8_t* this_field, uint8_t tag) override;
    void     read_tag_block(uint8_t* out, std::size_t out_size,
                            uint32_t fourcc)                   override;
    uint32_t read_uint32_fourcc(uint32_t fourcc)               override;
    std::string read_string(uint8_t tag)   override;
    void     prelude()                     override;
    bool     any_miss() const              override { return miss_; }

    /// Position-restore helper for callers that need to peek without losing
    /// the cursor.
    std::size_t cursor() const noexcept { return cursor_; }
    void        set_cursor(std::size_t c) noexcept { cursor_ = c; }

private:
    /// Scan forward for the next occurrence of `tag` starting from cursor_.
    /// On hit, leave cursor at the start of the payload and return true.
    bool seek_tag(uint8_t tag);
    /// Same but scan for a 4-byte fourcc.
    bool seek_fourcc(uint32_t fourcc);

    const uint8_t* data_;
    std::size_t    size_;
    std::size_t    cursor_;
    bool           miss_;
};

}  // namespace hdb
}  // namespace xfiles

#endif  // HDB_TLV_READER_H
