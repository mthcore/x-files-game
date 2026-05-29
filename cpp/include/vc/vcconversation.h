// SPDX-License-Identifier: MIT
// VCConversation — byte-exact 72-byte conversation record.
//
// Byte-direct size : 0x48 = 72 bytes
//
// TLV grammar :
//   read order  : 'e' 'f' 'h' 'j' 'g' 'i'
//   write order : 'e' 'f' 'g' 'h' 'i' 'j'  (canonical sorted)
//   layout      : +0x28 'e', +0x2c 'f', +0x30 'g', +0x34 'h', +0x38 'i', +0x3c 'j'

#ifndef VC_VCCONVERSATION_H
#define VC_VCCONVERSATION_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCConversation : public VCObject {
public:
    VCConversation() noexcept;
    ~VCConversation() override;

    void vt_slot_01() override {}
    void vt_slot_02() override {}
    void vt_slot_03() override {}
    void vt_slot_04() override {}
    void vt_slot_05() override {}
    char is_leaf() const override { return 1; }  // leaf (no sub-coll)
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

protected:
    // +0x04..+0x27 : base + intermediate (36 bytes)
    char _engine_internal[0x10];
    uint32_t _npfl_flags;
    uint32_t _vers;
    char _intermediate[0x0c];

public:
    // +0x28..+0x3f : 6 uint32 fields (tags 'e'..'j')
    uint32_t field_e;   // +0x28 'e'
    uint32_t field_f;   // +0x2c 'f'
    uint32_t field_g;   // +0x30 'g'
    uint32_t field_h;   // +0x34 'h'
    uint32_t field_i;   // +0x38 'i'
    uint32_t field_j;   // +0x3c 'j'

protected:
    // +0x40..+0x47 : 8-byte tail
    char _tail[0x08];
};

XFILES_ASSERT_SIZE(VCConversation, VCCONVERSATION_SIZE, "VCConversation byte-direct size mismatch");
XFILES_ASSERT_OFFSET(VCConversation, field_e, 0x28, "field_e offset");
XFILES_ASSERT_OFFSET(VCConversation, field_j, 0x3c, "field_j offset");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCCONVERSATION_H
