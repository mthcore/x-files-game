// SPDX-License-Identifier: MIT
// VCActionIcon - 68-byte Icon (member of the 5-class Icon family, vc_family_icon.md).
//
// Byte-direct format layout (trivial
// thunk to the shared reader). All Icon classes share this exact byte layout :
//   +0x00..+0x03 : vtable (class-specific)
//   +0x04..+0x27 : VCObject base scratch (36B)
//   +0x28 cache_e (uint32 tag 'e' 0x65) | +0x2C cache_f (tag 'f' 0x66)
//   +0x30 cache_g (tag 'g' 0x67)        | +0x34 cache_h (tag 'h' 0x68)
//   +0x38 cache_k (tag 'k' 0x6b, version-gated)
//   +0x3C cache_i (tag 'i' 0x69)        | +0x40 cache_j (tag 'j' 0x6a)

#ifndef VC_VCACTIONICON_H
#define VC_VCACTIONICON_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCActionIcon : public VCObject {
public:
    VCActionIcon() noexcept;
    ~VCActionIcon() override;

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

    uint8_t  _base_scratch[0x24];   // +0x04..+0x27 base
    uint32_t cache_e;               // +0x28 tag 'e'
    uint32_t cache_f;               // +0x2C tag 'f'
    uint32_t cache_g;               // +0x30 tag 'g'
    uint32_t cache_h;               // +0x34 tag 'h'
    uint32_t cache_k;               // +0x38 tag 'k' (version-gated)
    uint32_t cache_i;               // +0x3C tag 'i'
    uint32_t cache_j;               // +0x40 tag 'j'
};

XFILES_ASSERT_SIZE(VCActionIcon, VCACTIONICON_SIZE, "VCActionIcon size 0x44 mismatch");
XFILES_ASSERT_OFFSET(VCActionIcon, _base_scratch, 0x04, "_base_scratch offset");
XFILES_ASSERT_OFFSET(VCActionIcon, cache_e, 0x28, "cache_e offset");
XFILES_ASSERT_OFFSET(VCActionIcon, cache_f, 0x2C, "cache_f offset");
XFILES_ASSERT_OFFSET(VCActionIcon, cache_g, 0x30, "cache_g offset");
XFILES_ASSERT_OFFSET(VCActionIcon, cache_h, 0x34, "cache_h offset");
XFILES_ASSERT_OFFSET(VCActionIcon, cache_k, 0x38, "cache_k offset");
XFILES_ASSERT_OFFSET(VCActionIcon, cache_i, 0x3C, "cache_i offset");
XFILES_ASSERT_OFFSET(VCActionIcon, cache_j, 0x40, "cache_j offset");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCACTIONICON_H
