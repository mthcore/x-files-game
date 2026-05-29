// SPDX-License-Identifier: MIT
// VCInterfaceItem byte-direct deserializer implementation.

#include "vc/vc_interface_item_reader.h"

#include <cstring>

namespace xfiles {
namespace vc {

namespace {

uint16_t be16(const uint8_t* p) {
    return static_cast<uint16_t>((uint16_t(p[0]) << 8) | uint16_t(p[1]));
}
int16_t be_i16(const uint8_t* p) {
    return static_cast<int16_t>((uint16_t(p[0]) << 8) | uint16_t(p[1]));
}

// Decode the 28-byte bbox sub-object payload. Best-effort heuristic :
//   - Most VC bbox sub-objects start with a 4-byte vtable pointer (skipped)
//   - Followed by (x, y, w, h) as int16 in big-endian (the standard QT/PICT
//     rect ordering inherited by the HyperBole engine)
//   - Remaining bytes are flags / scratch
//
// We try two layouts and pick the one that yields a plausible rect (w/h > 0,
// fits in 640x480, etc.). If neither works, ok stays false and raw is kept.
VCBBox decode_bbox(const uint8_t* p, std::size_t n) {
    VCBBox bb;
    if (!p || n < 8) return bb;
    std::size_t copy = (n < sizeof(bb.raw)) ? n : sizeof(bb.raw);
    std::memcpy(bb.raw, p, copy);

    // Try layout A : 4 int16_t big-endian at offset 0 (x, y, w, h).
    int16_t a_x = be_i16(p);
    int16_t a_y = be_i16(p + 2);
    int16_t a_w = be_i16(p + 4);
    int16_t a_h = be_i16(p + 6);
    auto plausible = [](int16_t x, int16_t y, int16_t w, int16_t h) {
        return x >= 0 && y >= 0 && w > 0 && h > 0
            && x + w <= 800 && y + h <= 600;
    };
    if (plausible(a_x, a_y, a_w, a_h)) {
        bb.x = a_x; bb.y = a_y; bb.w = a_w; bb.h = a_h; bb.ok = true;
        return bb;
    }
    // Try layout B : skip 4-byte vtable ptr, rect at offset 4.
    if (n >= 12) {
        int16_t b_x = be_i16(p + 4);
        int16_t b_y = be_i16(p + 6);
        int16_t b_w = be_i16(p + 8);
        int16_t b_h = be_i16(p + 10);
        if (plausible(b_x, b_y, b_w, b_h)) {
            bb.x = b_x; bb.y = b_y; bb.w = b_w; bb.h = b_h; bb.ok = true;
            return bb;
        }
    }
    return bb;  // raw kept ; ok = false
}

}  // anon

bool read_vc_interface_item(hdb::TLVReader& reader, VCInterfaceItemData* out) {
    if (!out) return false;
    *out = {};
    reader.prelude();
    // The bbox sub-object at tag 'e' is read via vtable dispatch on a
    // sub-CString-like object. For now we peek the payload bytes via
    // a low-level helper : a separate read_sub_object dispatch path.
    // Since our MemoryTLVReader's read_sub_object only advances the
    // cursor, we work around by reading the tag block as raw bytes.
    {
        // Save cursor + try read tag 'e' payload via read_string-like path
        // (works because the bbox sub-object is stored as a length-prefixed
        // blob, even if its bytes aren't ASCII).
        auto* mem = static_cast<hdb::MemoryTLVReader*>(&reader);
        std::size_t save = mem->cursor();
        std::string sub = mem->read_string('e');
        out->bbox = decode_bbox(
            reinterpret_cast<const uint8_t*>(sub.data()), sub.size());
        (void)save;
    }
    out->field_44 = reader.read_uint32('f');
    out->field_48 = reader.read_uint32('g');
    out->field_4c = reader.read_uint16('h');
    // tag 'i' is only present in vers > 4 records ; treat as optional.
    uint16_t i_lo = reader.read_uint16('i');
    out->field_50 = static_cast<int32_t>(static_cast<int16_t>(i_lo));

    return !reader.any_miss()
        || out->bbox.ok
        || out->field_44 != 0 || out->field_48 != 0
        || out->field_4c != 0 || out->field_50 != 0;
}

}  // namespace vc
}  // namespace xfiles
