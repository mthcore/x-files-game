// SPDX-License-Identifier: MIT
// VCCharView — 92-byte character-view (scene-graph family).
//
// Byte-direct format layout :
//   +0x00..+0x03 vtable
//   +0x04..+0x27 VCObject base scratch (36B)
//   +0x28 cache_e (uint32 tag 'e' 0x65)
//   +0x2C..+0x47 sub-object (28B) — (this+0x2C)->vtable[+0x08](ctx, 'f')
//   +0x48 cache_g (uint32 tag 'g' 0x67)
//   +0x4C cache_h (uint32 tag 'h' 0x68)
//   +0x50 cache_i (uint32 tag 'i' 0x69)
//   +0x54 cache_j (uint32 tag 'j' 0x6a)
//   +0x58..+0x5B reserved/pad

#ifndef VC_VCCHAR_VIEW_H
#define VC_VCCHAR_VIEW_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCCharView : public VCObject {
public:
    VCCharView() noexcept;
    ~VCCharView() override;

    void vt_slot_01() override {} void vt_slot_02() override {} void vt_slot_03() override {}
    void vt_slot_04() override {} void vt_slot_05() override {}
    char is_leaf() const override { return 0; }
    void vt_slot_07() override {} void vt_slot_08() override {}
    VCObject* id_lookup(uint32_t) override { return nullptr; }
    void vt_slot_10() override {}
    uint32_t eval(uint32_t, uint32_t) override { return 0; }
    void Read(HDBContext*, uint32_t) override;
    void Write(HDBContext*) const override;
    void vt_slot_14() override {} void vt_slot_15() override {} void vt_slot_16() override {}
    void vt_slot_17() override {} void vt_slot_18() override {} void vt_slot_19() override {}
    void vt_slot_20() override {} void vt_slot_21() override {} void vt_slot_22() override {}
    void vt_slot_23() override {} void vt_slot_24() override {} void vt_slot_25() override {}
    void vt_slot_26() override {} void vt_slot_27() override {}

    uint8_t  _base_scratch[0x24];
    uint32_t cache_e;               // +0x28
    uint8_t  sub_object[0x1C];      // +0x2C..+0x47 (28B)
    uint32_t cache_g;               // +0x48
    uint32_t cache_h;               // +0x4C
    uint32_t cache_i;               // +0x50
    uint32_t cache_j;               // +0x54
    uint32_t field_58;              // +0x58 reserved
};

XFILES_ASSERT_SIZE(VCCharView, VCCHARVIEW_SIZE, "VCCharView size 0x5c");
XFILES_ASSERT_OFFSET(VCCharView, cache_e, 0x28, "cache_e");
XFILES_ASSERT_OFFSET(VCCharView, sub_object, 0x2C, "sub_object");
XFILES_ASSERT_OFFSET(VCCharView, cache_g, 0x48, "cache_g");
XFILES_ASSERT_OFFSET(VCCharView, cache_j, 0x54, "cache_j");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCCHAR_VIEW_H
