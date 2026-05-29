// SPDX-License-Identifier: MIT
// VCLocaton — 120-byte location node (scene-graph family ; typo "Locaton"
// matches the on-disk format).
//
// Byte-direct format layout :
//   +0x00..+0x03 vtable
//   +0x04..+0x27 VCObject base scratch (36B)
//   +0x28..+0x63 sub-collection (60B) —(0x65)
//   +0x64 cache_f (tag 'f' 0x66)
//   +0x68 cache_g (tag 'g' 0x67)
//   +0x6C cache_h (tag 'h' 0x68)
//   +0x70 field_70 (uninitialised in Read — 4B reserved)
//   +0x74 cache_i (byte tag 'i' 0x69)
//   +0x75..+0x77 pad

#ifndef VC_VCLOCATON_H
#define VC_VCLOCATON_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCLocaton : public VCObject {
public:
    VCLocaton() noexcept;
    ~VCLocaton() override;

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
    uint8_t  sub_coll[0x3C];        // +0x28..+0x63
    uint32_t cache_f;               // +0x64 tag 'f'
    uint32_t cache_g;               // +0x68 tag 'g'
    uint32_t cache_h;               // +0x6C tag 'h'
    uint32_t field_70;              // +0x70 reserved
    uint8_t  cache_i;               // +0x74 byte tag 'i'
    uint8_t  _pad_75[3];
};

XFILES_ASSERT_SIZE(VCLocaton, VCLOCATON_SIZE, "VCLocaton size 0x78");
XFILES_ASSERT_OFFSET(VCLocaton, sub_coll, 0x28, "sub_coll");
XFILES_ASSERT_OFFSET(VCLocaton, cache_f, 0x64, "cache_f");
XFILES_ASSERT_OFFSET(VCLocaton, cache_i, 0x74, "cache_i");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCLOCATON_H
