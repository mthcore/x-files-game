// SPDX-License-Identifier: MIT
// VCExplorable — 76-byte explorable scene node.
//
// Byte-direct format layout :
//   +0x00..+0x03 : vtable ()
//   +0x04..+0x27 : VCObject base scratch (36B)
//   +0x28..+0x2B : sub_object ptr (4B) — responds to tag 'e' (0x65)
//   +0x2C..+0x37 : fields (12B) — sub-object internal
//   +0x38..+0x3B : heap_owned ptr (4B) — init 0 in ctor, freed in dtor
//   +0x3C..+0x43 : fields (8B)
//   +0x44..+0x47 : cache_f (uint32, tag 'f' 0x66)
//   +0x48..+0x4B : field_48 (uint32 = 0 in ctor)

#ifndef VC_VCEXPLORABLE_H
#define VC_VCEXPLORABLE_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCExplorable : public VCObject {
public:
    VCExplorable() noexcept;
    ~VCExplorable() override;

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

    uint8_t  _base_scratch[0x24];   // +0x04..+0x27 VCObject base
    uint32_t sub_object_ptr;        // +0x28 sub-object reference
    uint8_t  sub_fields[0x0C];      // +0x2C..+0x37
    uint32_t heap_owned;            // +0x38 heap_owned ptr (init 0)
    uint8_t  more_fields[0x08];     // +0x3C..+0x43
    uint32_t cache_f;               // +0x44 tag 'f' (0x66)
    uint32_t field_48;              // +0x48 = 0
};

XFILES_ASSERT_SIZE(VCExplorable, VCEXPLORABLE_SIZE, "VCExplorable size 0x4c mismatch");
XFILES_ASSERT_OFFSET(VCExplorable, _base_scratch, 0x04, "_base_scratch offset");
XFILES_ASSERT_OFFSET(VCExplorable, sub_object_ptr, 0x28, "sub_object_ptr offset");
XFILES_ASSERT_OFFSET(VCExplorable, sub_fields, 0x2C, "sub_fields offset");
XFILES_ASSERT_OFFSET(VCExplorable, heap_owned, 0x38, "heap_owned offset");
XFILES_ASSERT_OFFSET(VCExplorable, more_fields, 0x3C, "more_fields offset");
XFILES_ASSERT_OFFSET(VCExplorable, cache_f, 0x44, "cache_f offset");
XFILES_ASSERT_OFFSET(VCExplorable, field_48, 0x48, "field_48 offset");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCEXPLORABLE_H
