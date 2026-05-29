// SPDX-License-Identifier: MIT
// VCCursor — 60-byte mouse-cursor descriptor (sprite name + 2 scalar fields).
//
// Byte-direct from the format notes +
// Read :
//   +0x00..+0x03 : vtable ()
//   +0x04..+0x27 : VCObject base scratch (36B)
//   +0x28..+0x33 : sub-object (CString sprite asset name, 12B)
//                 — ctor'd by, read via sub_vt[+0x08]('e' tag 0x65)
//   +0x34 : uint32 field, tag 'f' (0x66) — cursor state cache
//   +0x38 : uint32 field, tag 'g' (0x67) — cursor secondary cache

#ifndef VC_VCCURSOR_H
#define VC_VCCURSOR_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCCursor : public VCObject {
public:
    VCCursor() noexcept;
    ~VCCursor() override;

    void vt_slot_01() override {} void vt_slot_02() override {} void vt_slot_03() override {}
    void vt_slot_04() override {} void vt_slot_05() override {}
    char is_leaf() const override { return 0; }
    void vt_slot_07() override {} void vt_slot_08() override {}
    VCObject* id_lookup(uint32_t) override { return nullptr; }
    void vt_slot_10() override {}
    uint32_t eval(uint32_t, uint32_t) override { return 0; }
    void Read(HDBContext*, uint32_t) override;            // slot 12
    void Write(HDBContext*) const override;
    void vt_slot_14() override {} void vt_slot_15() override {} void vt_slot_16() override {}
    void vt_slot_17() override {} void vt_slot_18() override {} void vt_slot_19() override {}
    void vt_slot_20() override {} void vt_slot_21() override {} void vt_slot_22() override {}
    void vt_slot_23() override {} void vt_slot_24() override {} void vt_slot_25() override {}
    void vt_slot_26() override {} void vt_slot_27() override {}

    // +0x04..+0x27 : VCObject base scratch (36B).
    uint8_t  _base_scratch[0x24];

    // +0x28..+0x33 : 12-byte sub-object (CString sprite name).
    uint8_t  sprite_string[0x0C];

    // +0x34 : tag 'f' (0x66) scalar field — cursor state.
    uint32_t cache_f;

    // +0x38 : tag 'g' (0x67) scalar field — cursor secondary state.
    uint32_t cache_g;
};

XFILES_ASSERT_SIZE(VCCursor, VCCURSOR_SIZE, "VCCursor size 0x3c mismatch");
XFILES_ASSERT_OFFSET(VCCursor, _base_scratch, 0x04, "_base_scratch offset");
XFILES_ASSERT_OFFSET(VCCursor, sprite_string, 0x28, "sprite_string offset");
XFILES_ASSERT_OFFSET(VCCursor, cache_f, 0x34, "cache_f offset");
XFILES_ASSERT_OFFSET(VCCursor, cache_g, 0x38, "cache_g offset");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCCURSOR_H
