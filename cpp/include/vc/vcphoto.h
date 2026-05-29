// SPDX-License-Identifier: MIT
// VCPhoto — byte-exact 152-byte photo record.
//
// Byte-direct size : 0x98 = 152 bytes
//
// TLV grammar :
//   sub-coll at +0x28, tag 'e' (0x65)
//   uint8 at +0x94, tag 'i' (0x69)
//   uint8 at +0x95, tag 'j' (0x6a)

#ifndef VC_VCPHOTO_H
#define VC_VCPHOTO_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"
#include "vc/inline_collection.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCPhoto : public VCObject {
public:
    VCPhoto() noexcept;
    ~VCPhoto() override;

    void vt_slot_01() override {}
    void vt_slot_02() override {}
    void vt_slot_03() override {}
    void vt_slot_04() override {}
    void vt_slot_05() override {}
    char is_leaf() const override { return 0; }
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
    // +0x28 : embedded sub-coll (60 bytes, photo content items)
    InlineCollection sub_coll;

protected:
    // +0x64..+0x93 : 48-byte tail data
    char _tail0[0x30];

public:
    // +0x94 : 'i' field (uint8)
    uint8_t field_i;
    // +0x95 : 'j' field (uint8)
    uint8_t field_j;

protected:
    // +0x96..+0x97 : trailing padding
    char _tail1[0x02];
};

XFILES_ASSERT_SIZE(VCPhoto, VCPHOTO_SIZE, "VCPhoto size mismatch");
XFILES_ASSERT_OFFSET(VCPhoto, sub_coll, 0x28, "sub_coll offset");
XFILES_ASSERT_OFFSET(VCPhoto, field_i, 0x94, "field_i offset");
XFILES_ASSERT_OFFSET(VCPhoto, field_j, 0x95, "field_j offset");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCPHOTO_H
