// SPDX-License-Identifier: MIT
// NL HDBSerializer — byte-direct port of the typed reader used by every
// VCObject::Read body in the on-disk format.
//
// In the on-disk format, each VC* class's Read method receives a pointer to
// an HDBSerializer instance (a CNeoPersist subclass) and pulls field values
// out by tag via vtable dispatch :
//
//
// The eight slots observed across all 51 VC* Read bodies + the base init
// (the on-disk Read sequence) are :
//
//   +0x14  read_string(tag) -> std::string  (variable-length, rare)
//   +0x74  read_buf_fourcc(dest, count, fc) (used by base init for 'NPfl')
//   +0x7c  read_byte_alt(tag) -> uint8_t    (alt byte reader, v3+)
//   +0x80  read_byte(tag) -> uint8_t        (bool, small enum)
//   +0x9c  read_dword(tag) -> uint32_t      (uint32, handle, id)
//   +0xa8  begin_record()                    (called once at Read start)
//   +0xb0  read_short(tag) -> int32_t       (int16 sign-extended)
//   +0xbc  read_dword_fourcc(fc) -> uint32_t (dword tagged by 4-byte fourcc)
//
// Tag semantics :
//   - byte tag : single byte (0x65='e', 0x66='f', ...) — used by_class
//     bodies for per-field reads ; matches Hungarian naming convention
//     of the original C++ source.
//   - fourcc tag : 4-byte ASCII marker (e.g., 'NPfl' = 0x4e50666c,
//     'vers' = 0x76657273) — used by base/header readers to verify the
//     record structure.
//
// The on-disk TLV wire format is sequential : each typed read consumes
// `[tag_bytes][value_bytes]` from the stream cursor. Field order in the
// stream matches the order the Read body calls the typed slots (NOT tag
// order, NOT struct-offset order). Endianness for primitive values is
// little-endian (HyperBole / NeoLogic convention for record payloads ;
// HDB container header is big-endian, decoded elsewhere).
//
// This header declares an abstract `HDBSerializer` interface (mirrors the
// vtable contract) plus a concrete `InMemoryHDBSerializer` that reads
// from a `const uint8_t*` buffer. this is wired up so unit
// tests can exercise the contract on synthetic blobs ; later phases will
// add `HDBSerializer` adapters over real HDB leaf payloads.

#ifndef NL_HDB_SERIALIZER_H
#define NL_HDB_SERIALIZER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace xfiles {
namespace nl {

/// Abstract HDBSerializer interface. Each method maps to one vtable slot
/// of the on-disk CNeoPersist-derived serializer (offsets are byte
/// offsets within the vtable, see file-level comment).
class HDBSerializer {
public:
    virtual ~HDBSerializer() = default;

    /// Slot +0x14 : read a variable-length string tagged with `tag`.
    virtual std::string read_string(uint8_t tag) = 0;

    /// Slot +0x74 : read `count` bytes into `dest` guarded by a fourcc
    /// marker. Used by base init to grab the NPfl flags.
    virtual void read_buf_fourcc(void* dest, std::size_t count,
                                 uint32_t fourcc) = 0;

    /// Slot +0x7c : alternate byte reader (v3+ records).
    virtual uint8_t read_byte_alt(uint8_t tag) = 0;

    /// Slot +0x80 : read a single byte (uint8) tagged with `tag`.
    virtual uint8_t read_byte(uint8_t tag) = 0;

    /// Slot +0x9c : read a uint32 (LE) tagged with `tag`.
    virtual uint32_t read_dword(uint8_t tag) = 0;

    /// Slot +0xa8 : called once at the start of each Read body to open
    /// the record / advance the cursor past any session header. In the
    /// in-memory serializer this is a no-op.
    virtual void begin_record() = 0;

    /// Slot +0xb0 : read an int16 (LE) tagged with `tag`, returned
    /// sign-extended to int32 (matches on-disk return convention).
    virtual int32_t read_short(uint8_t tag) = 0;

    /// Slot +0xbc : read a uint32 (LE) tagged with a 4-byte fourcc.
    /// Different from `read_dword` which uses a single byte tag.
    virtual uint32_t read_dword_fourcc(uint32_t fourcc) = 0;

    /// True if any read has hit an error (tag mismatch, EOF mid-read,
    /// fourcc mismatch). Subsequent reads return zero/empty.
    virtual bool error() const = 0;

    /// Optional diagnostic : current cursor position. Default 0.
    virtual std::size_t cursor() const { return 0; }

    /// Current record version. Set by the loader before invoking each
    /// VCObject::Read. Conditional fields gate on this value :
    ///   - V > 2 adds e.g. 'g' / 'i' tags
    ///   - V > 3 adds 'k' (byte) etc.
    /// On disk, the format stores this at `param_1[3] + 4` ; subclass Reads
    /// dereference it directly (see the on-disk Read sequence VCConversation).
    virtual uint32_t current_version() const = 0;

    /// True if the current class's metadata declares a version field.
    /// In the on-disk format this is `param_1[3]->byte_at_0xf != 0`.
    /// All 9 save classes have it ; non-save classes may not.
    virtual bool has_version_field() const = 0;

    // -----------------------------------------------------------------
    // Write side (mirror of read_*) — for save round-trip.
    // The on-disk format's VCObject::Write (slot 13) calls these in order
    // matching Read. Each write_* must produce bytes that the matching
    // read_* would consume identically.
    // -----------------------------------------------------------------

    /// Slot +0x18 (write) : start of record. No-op in InMemory.
    virtual void begin_write_record() {}

    /// Slot +0x74 (write) : write `count` bytes from `src` guarded by
    /// fourcc. Mirror of read_buf_fourcc.
    virtual void write_buf_fourcc(const void* src, std::size_t count,
                                  uint32_t fourcc) = 0;

    /// Slot +0x80 (write) : write a uint8 tagged with byte tag.
    virtual void write_byte(uint8_t tag, uint8_t v) = 0;

    /// Slot +0xb0 (write) : write an int16 (LE) tagged with byte tag.
    virtual void write_short(uint8_t tag, int32_t v) = 0;

    /// Slot +0x9c (write) : write a uint32 (LE) tagged with byte tag.
    virtual void write_dword(uint8_t tag, uint32_t v) = 0;

    /// Slot +0xbc (write) : write a uint32 (LE) tagged with fourcc.
    virtual void write_dword_fourcc(uint32_t fourcc, uint32_t v) = 0;

    /// Slot +0x14 (write) : write a variable-length string tagged by tag.
    virtual void write_string(uint8_t tag, const std::string& s) = 0;
};

/// Concrete HDBSerializer over a raw byte buffer. Reads sequentially
/// according to the TLV wire contract described in the file comment.
class InMemoryHDBSerializer : public HDBSerializer {
public:
    /// Construct over a buffer ; the buffer must outlive the serializer.
    InMemoryHDBSerializer(const uint8_t* data, std::size_t size);

    std::string read_string(uint8_t tag) override;
    void        read_buf_fourcc(void* dest, std::size_t count,
                                uint32_t fourcc) override;
    uint8_t     read_byte_alt(uint8_t tag) override;
    uint8_t     read_byte(uint8_t tag) override;
    uint32_t    read_dword(uint8_t tag) override;
    void        begin_record() override;
    int32_t     read_short(uint8_t tag) override;
    uint32_t    read_dword_fourcc(uint32_t fourcc) override;

    bool        error() const override { return error_; }
    std::size_t cursor() const override { return cursor_; }
    std::size_t size() const { return size_; }
    bool        eof() const { return cursor_ >= size_; }

    /// Reset cursor + error state (test helper).
    void reset();

    uint32_t current_version() const override { return version_; }
    bool     has_version_field() const override { return has_version_; }

    /// Test helpers : set the class-meta-derived properties the
    /// serializer carries before invoking VCObject::Read.
    void set_current_version(uint32_t v)    { version_ = v; }
    void set_has_version_field(bool has)    { has_version_ = has; }

    // --- Write side : append to an internal output buffer -------------
    void        write_buf_fourcc(const void* src, std::size_t count,
                                 uint32_t fourcc) override;
    void        write_byte(uint8_t tag, uint8_t v) override;
    void        write_short(uint8_t tag, int32_t v) override;
    void        write_dword(uint8_t tag, uint32_t v) override;
    void        write_dword_fourcc(uint32_t fourcc, uint32_t v) override;
    void        write_string(uint8_t tag, const std::string& s) override;

    /// Access the bytes accumulated by write_*.
    const std::vector<uint8_t>& output() const { return output_; }
    void                        clear_output()  { output_.clear(); }

private:
    const uint8_t* data_;
    std::size_t    size_;
    std::size_t    cursor_;
    bool           error_;
    uint32_t       version_     = 0;
    bool           has_version_ = true;
    std::vector<uint8_t> output_;

    bool expect_byte_tag(uint8_t expected);
    bool expect_fourcc_tag(uint32_t expected);
    bool ensure_remaining(std::size_t n);
    uint16_t read_u16_le_raw();
    uint32_t read_u32_le_raw();
    int16_t  read_i16_le_raw();
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_HDB_SERIALIZER_H
