// SPDX-License-Identifier: MIT
// HDB container — NeoLogic Hammer Database format.
//
// HDB is the on-disk format's container format for game data and save state :
//   - 256-byte pages
//   - First page = header
//   - B-tree index for record lookup
//   - Records contain TLV-formatted serialized VC objects
//
// XFILES.GAM (save game) = HDB container, 152 KB, 4 save-state classes.
//
// Reference :
//   On-disk format (NeoPersist 3.0).
//   scripts/gam_field_aware_roundtrip.py — Python reference impl

#ifndef HDB_CONTAINER_H
#define HDB_CONTAINER_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace xfiles {
namespace hdb {

constexpr std::size_t PAGE_SIZE = 256;
constexpr std::size_t HEADER_SIZE = 32;

/// HDB page-type tags (first byte of each page). Discovered via dump
/// statistics on XFILES.HDB + XFILES.GAM (W3 inspect).
enum class PageTag : uint8_t {
    Empty             = 0x00,
    BtreeFreed        = 0xc0,
    BtreeInternal     = 0xc2,
    BtreeLeaf         = 0xc3,
    BtreeInternalAlt  = 0xd2,
    // Other values (0x41..0x5a, 0x61..0x7a = ASCII A-Z, a-z) indicate a *data*
    // page that begins with a NUL-terminated key string.
};

/// Page = 256 bytes of raw HDB data.
struct Page {
    uint8_t bytes[PAGE_SIZE];
};

/// 8-byte typed record. Universal layout observed in all HDB/GAM pages :
///   +0x00 : tag      (u8)
///   +0x01 : sub_tag  (u8)
///   +0x02 : flag16   (BE u16)
///   +0x04 : value32  (BE u32)
struct Record8 {
    uint8_t  tag;
    uint8_t  sub_tag;
    uint16_t flag16;   ///< Decoded big-endian
    uint32_t value32;  ///< Decoded big-endian

    /// Re-serialize back to 8 bytes (big-endian). Used to validate
    /// byte-identical round-trip.
    void emit(uint8_t out[8]) const {
        out[0] = tag;
        out[1] = sub_tag;
        out[2] = static_cast<uint8_t>(flag16 >> 8);
        out[3] = static_cast<uint8_t>(flag16 & 0xff);
        out[4] = static_cast<uint8_t>((value32 >> 24) & 0xff);
        out[5] = static_cast<uint8_t>((value32 >> 16) & 0xff);
        out[6] = static_cast<uint8_t>((value32 >>  8) & 0xff);
        out[7] = static_cast<uint8_t>( value32        & 0xff);
    }

    static Record8 parse(const uint8_t in[8]) {
        Record8 r;
        r.tag     = in[0];
        r.sub_tag = in[1];
        r.flag16  = static_cast<uint16_t>((uint16_t(in[2]) << 8) | uint16_t(in[3]));
        r.value32 = (uint32_t(in[4]) << 24) | (uint32_t(in[5]) << 16)
                  | (uint32_t(in[6]) <<  8) |  uint32_t(in[7]);
        return r;
    }
};

/// Walk a 256-byte page and emit typed Record8 entries. For data pages with
/// letter-prefix first byte (= they start with a NUL-terminated key string),
/// `key_string` captures the leading ASCII (NUL-terminated input), and
/// `key_padded_len` captures the number of bytes consumed before the first
/// 8-byte record (rounded up to the next 8-byte boundary).
/// Returns the number of Record8 appended to `out`.
std::size_t hdb_parse_page(const Page& page,
                           std::vector<Record8>* out,
                           std::string* key_string = nullptr,
                           std::size_t* key_padded_len = nullptr);

/// HDB container — owns pages + provides byte-direct access.
///
/// Usage :
///   HDBContainer c;
///   c.load_from_file("XFILES.GAM");
///   ... read records via TLV interface
///   c.save_to_file("XFILES.GAM.out");  // byte-identique round-trip if no modifications
class HDBContainer {
public:
    HDBContainer();
    ~HDBContainer();

    /// Load HDB file. Returns true on success.
    bool load_from_file(const std::string& path);

    /// Save HDB file. Round-trip byte-identique if no modifications.
    bool save_to_file(const std::string& path) const;

    /// Raw access to underlying bytes (header + pages + trailer).
    const std::vector<uint8_t>& raw_bytes() const { return raw_; }

    /// Number of pages (after header).
    std::size_t page_count() const;

    /// Access page by index.
    const Page& page(std::size_t idx) const;

    /// Header bytes (first HEADER_SIZE bytes).
    const uint8_t* header() const { return raw_.data(); }

    /// Trailer bytes (after last page).
    const uint8_t* trailer() const;
    std::size_t trailer_size() const;

private:
    std::vector<uint8_t> raw_;
};

}  // namespace hdb
}  // namespace xfiles

#endif  // HDB_CONTAINER_H
