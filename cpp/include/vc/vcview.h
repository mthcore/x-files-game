// SPDX-License-Identifier: MIT
// VCView — 68-byte view (camera frame) descriptor in scene-graph family.
//
// Byte-direct format layout :
//   +0x00..+0x03 vtable
//   +0x04..+0x27 VCObject base scratch (36B)
//   +0x28 cache_e (tag 'e' 0x65)  | +0x2C cache_f (tag 'f' 0x66)
//   +0x30 cache_g (tag 'g' 0x67)  | +0x34 cache_h (tag 'h' 0x68)
//   +0x38 cache_i (tag 'i' 0x69)  | +0x3C cache_j (tag 'j' 0x6a)

#ifndef VC_VCVIEW_H
#define VC_VCVIEW_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCView : public VCObject {
public:
    VCView() noexcept;
    ~VCView() override;

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
    uint32_t cache_e, cache_f, cache_g, cache_h, cache_i, cache_j;
    uint32_t field_40;              // +0x40 ctor-only (zeroed, never read)
};

XFILES_ASSERT_SIZE(VCView, VCVIEW_SIZE, "VCView size 0x44");
XFILES_ASSERT_OFFSET(VCView, _base_scratch, 0x04, "_base_scratch");
XFILES_ASSERT_OFFSET(VCView, cache_e, 0x28, "cache_e");
XFILES_ASSERT_OFFSET(VCView, cache_j, 0x3C, "cache_j");
XFILES_ASSERT_OFFSET(VCView, field_40, 0x40, "field_40");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCVIEW_H
