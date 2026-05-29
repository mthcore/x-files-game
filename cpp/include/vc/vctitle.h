// SPDX-License-Identifier: MIT
// VCTitle - 168B, byte-direct Read decoder.
//   +0x28..+0x63 sub-coll (60B, version > 1 only, tag e via)
//   +0x64..+0x73 sub_object_f (16B, tag f)
//   +0x74..+0x83 sub_object_g (16B, tag g)
//   +0x84..+0x8B sub_object_h (8B, tag h)
//   +0x8C..+0x9B sub_object_i (16B, tag i)
//   +0x9C cache_j (uint32 tag j)
//   +0xA0 cache_k (uint32 tag k)
//   +0xA4 cache_l (uint16 tag l)
//   +0xA6 cache_m (uint16 tag m)

#ifndef VC_VCTITLE_H
#define VC_VCTITLE_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCTitle : public VCObject {
public:
    VCTitle() noexcept;
    ~VCTitle() override;

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
    uint8_t  sub_object_f[0x10];    // +0x64..+0x73
    uint8_t  sub_object_g[0x10];    // +0x74..+0x83
    uint8_t  sub_object_h[0x08];    // +0x84..+0x8B
    uint8_t  sub_object_i[0x10];    // +0x8C..+0x9B
    uint32_t cache_j;               // +0x9C
    uint32_t cache_k;               // +0xA0
    uint16_t cache_l;               // +0xA4
    uint16_t cache_m;               // +0xA6
};

XFILES_ASSERT_SIZE(VCTitle, VCTITLE_SIZE, "VCTitle 0xa8");
XFILES_ASSERT_OFFSET(VCTitle, sub_coll, 0x28, "sub_coll");
XFILES_ASSERT_OFFSET(VCTitle, sub_object_f, 0x64, "f");
XFILES_ASSERT_OFFSET(VCTitle, sub_object_g, 0x74, "g");
XFILES_ASSERT_OFFSET(VCTitle, sub_object_h, 0x84, "h");
XFILES_ASSERT_OFFSET(VCTitle, sub_object_i, 0x8C, "i");
XFILES_ASSERT_OFFSET(VCTitle, cache_j, 0x9C, "cache_j");
XFILES_ASSERT_OFFSET(VCTitle, cache_k, 0xA0, "cache_k");
XFILES_ASSERT_OFFSET(VCTitle, cache_l, 0xA4, "cache_l");
XFILES_ASSERT_OFFSET(VCTitle, cache_m, 0xA6, "cache_m");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCTITLE_H
