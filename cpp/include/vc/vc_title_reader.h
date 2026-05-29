// SPDX-License-Identifier: MIT
// VCTitle byte-direct deserializer ( body port).
//
// VCTitle is the singleton root object that holds the main menu state.
// Its 168-byte byte layout (per the format notes) :
//
//   offset  size  field         TLV tag  comment
//   ------- ----  -----------   -------  ---------------------------
//   +0x00   4     vtable
//   +0x04   0x24  base (VCAssetRefList) — read via
//   +0x64   0x10  string_f      'f'      sub-CString
//   +0x74   0x10  string_g      'g'      sub-CString
//   +0x84   0x08  string_h      'h'      sub-CString
//   +0x8c   0x10  string_i      'i'      sub-CString
//   +0x9c   4     field_9c      'j'      uint32
//   +0xa0   4     field_a0      'k'      uint32
//   +0xa4   2     field_a4      'l'      uint16
//   +0xa6   2     field_a6      'm'      uint16
//
// VCTitle::Read (slot 12) :
//   - NPfl+vers prelude
//   - if vers > 1 : reads tag 'e' (version-specific extra)
//   - reads 4 sub-objects via vtable[+0x8] (sub-CString Read)
//   - reads 4 numerics via vtable[+0x9c]/[+0xb0] (uint32/uint16 by tag)

#ifndef VC_TITLE_READER_H
#define VC_TITLE_READER_H

#include "hdb/tlv_reader.h"

#include <cstdint>
#include <string>

namespace xfiles {
namespace vc {

/// Decoded VCTitle data — the 4 strings + 4 numerics deserialized from HDB.
struct VCTitleData {
    std::string string_f;     // tag 'f' @ +0x64
    std::string string_g;     // tag 'g' @ +0x74
    std::string string_h;     // tag 'h' @ +0x84
    std::string string_i;     // tag 'i' @ +0x8c
    uint32_t    field_9c = 0; // tag 'j'
    uint32_t    field_a0 = 0; // tag 'k'
    uint16_t    field_a4 = 0; // tag 'l'
    uint16_t    field_a6 = 0; // tag 'm'
};

/// Read a VCTitle from a TLV reader. Returns true if any tag was found.
/// Byte-direct iso. The reader is positioned past the
/// VCTitle data on success.
bool read_vc_title(hdb::TLVReader& reader, VCTitleData* out);

}  // namespace vc
}  // namespace xfiles

#endif  // VC_TITLE_READER_H
