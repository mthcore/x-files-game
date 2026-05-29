// SPDX-License-Identifier: MIT
// VCInterfaceItem byte-direct deserializer ( body port).
//
// Each VCInterfaceItem is a clickable UI element (84 bytes) :
//
//   offset  size  field      TLV tag  comment
//   ------- ----  --------   -------  ----------------------------------
//   +0x00   4     vtable
//   +0x04   0x24  base NPfl+vers
//   +0x28   0x1c  bbox       'e'      sub-object (bounding rect + flags)
//   +0x44   4     field_44   'f'      uint32 (probably action_id / linked id)
//   +0x48   4     field_48   'g'      uint32 (probably trigger_id)
//   +0x4c   2     field_4c   'h'      uint16 (probably state/flags)
//   +0x50   4     field_50   'i'      int32 from int16 (only if vers > 4)
//
// The bbox sub-object at +0x28 is shared with VCHotSpot via 
// Its internal layout (28 B = 7 dwords) is best-effort decoded as 4 int16
// (x, y, w, h) + flags in the remaining bytes ; alternative decoding as
// 4 int32 not yet ruled out.

#ifndef VC_INTERFACE_ITEM_READER_H
#define VC_INTERFACE_ITEM_READER_H

#include "hdb/tlv_reader.h"

#include <cstdint>

namespace xfiles {
namespace vc {

/// Decoded bounding rect from the +0x28 sub-object. Coordinates are in
/// pixel space (signed). The decoding strategy is "best-effort" until the
/// sub-object Read is fully ported.
struct VCBBox {
    int16_t x = 0;
    int16_t y = 0;
    int16_t w = 0;
    int16_t h = 0;
    /// Bytes of the sub-object payload (for callers that want to inspect
    /// the un-decoded part).
    uint8_t  raw[28] = {};
    bool     ok = false;
};

/// Decoded VCInterfaceItem.
struct VCInterfaceItemData {
    VCBBox   bbox;
    uint32_t field_44 = 0;   // 'f' — likely action_id
    uint32_t field_48 = 0;   // 'g' — likely trigger_id
    uint16_t field_4c = 0;   // 'h' — state/flags
    int32_t  field_50 = 0;   // 'i' — only if vers > 4
};

/// Read a VCInterfaceItem from a TLV reader. Byte-direct iso of
///  Returns true if any tag was hit.
bool read_vc_interface_item(hdb::TLVReader& reader, VCInterfaceItemData* out);

}  // namespace vc
}  // namespace xfiles

#endif  // VC_INTERFACE_ITEM_READER_H
