// SPDX-License-Identifier: MIT
// VCEvidenceIcon — byte-exact (identical layout to VCInventory).
//
// Byte-direct size : 0x44 = 68 bytes
// 
//  (thunks to the shared Icon reader — shared with VCInventory)
//
// Same 7-field uint32 TLV grammar as VCInventory (W23.4).

#ifndef VC_VCEVIDENCEICON_H
#define VC_VCEVIDENCEICON_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCEvidenceIcon : public VCObject {
public:
    VCEvidenceIcon() noexcept;
    ~VCEvidenceIcon() override;

    void vt_slot_01() override {}
    void vt_slot_02() override {}
    void vt_slot_03() override {}
    void vt_slot_04() override {}
    void vt_slot_05() override {}
    char is_leaf() const override { return 1; }
    void vt_slot_07() override {}
    void vt_slot_08() override {}
    VCObject* id_lookup(uint32_t) override { return nullptr; }
    void vt_slot_10() override {}
    uint32_t eval(uint32_t, uint32_t) override { return 0; }
    void Read(HDBContext* ctx, uint32_t param_2) override;
    void Write(HDBContext* ctx) const override;
    void vt_slot_14() override {}
    void vt_slot_15() override {}
    void vt_slot_16() override {}
    void vt_slot_17() override {}
    void vt_slot_18() override {}
    void vt_slot_19() override {}
    void vt_slot_20() override {}
    void vt_slot_21() override {}
    void vt_slot_22() override {}
    void vt_slot_23() override {}
    void vt_slot_24() override {}
    void vt_slot_25() override {}
    void vt_slot_26() override {}
    void vt_slot_27() override {}

private:
    char _engine_internal_0[0x10];     // +0x04..+0x13
    uint32_t _npfl_flags;              // +0x14
    uint32_t _vers;                    // +0x18
    char _intermediate_0[0x0c];        // +0x1c..+0x27

public:
    uint32_t field_e;                  // +0x28 tag 'e'
    uint32_t field_f;                  // +0x2c tag 'f'
    uint32_t field_g;                  // +0x30 tag 'g'
    uint32_t field_h;                  // +0x34 tag 'h'
    uint32_t field_k;                  // +0x38 tag 'k'
    uint32_t field_i;                  // +0x3c tag 'i'
    uint32_t field_j;                  // +0x40 tag 'j'
};

XFILES_ASSERT_SIZE(VCEvidenceIcon, VCEVIDENCEICON_SIZE, "VCEvidenceIcon byte-direct size mismatch");
XFILES_ASSERT_OFFSET(VCEvidenceIcon, field_e, 0x28, "field_e offset mismatch");
XFILES_ASSERT_OFFSET(VCEvidenceIcon, field_j, 0x40, "field_j offset mismatch");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCEVIDENCEICON_H
