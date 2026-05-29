// SPDX-License-Identifier: MIT
// VCCharacter — 44-byte character descriptor.
//
// Byte-direct format layout :
//   +0x00..+0x03 : vtable
//   +0x04..+0x27 : VCObject base scratch (36B)
//   +0x28..+0x2B : cache_e (uint32 tag 'e' 0x65)

#ifndef VC_VCCHARACTER_H
#define VC_VCCHARACTER_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCCharacter : public VCObject {
public:
    VCCharacter() noexcept;
    ~VCCharacter() override;

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

    uint8_t  _base_scratch[0x24];   // +0x04..+0x27 base
    uint32_t cache_e;               // +0x28 tag 'e' (0x65)
};

XFILES_ASSERT_SIZE(VCCharacter, VCCHARACTER_SIZE, "VCCharacter size 0x2c mismatch");
XFILES_ASSERT_OFFSET(VCCharacter, _base_scratch, 0x04, "_base_scratch offset");
XFILES_ASSERT_OFFSET(VCCharacter, cache_e, 0x28, "cache_e offset");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCCHARACTER_H
