// SPDX-License-Identifier: MIT
// VCAssetRef - 68B, byte-direct Read decoder.
//   +0x28..+0x37 sub_object (16B) - tag e
//   +0x38 cache_f (uint32 tag f)
//   +0x3C cache_g (uint32 tag g)
//   +0x40 cache_h (byte tag h)
//   +0x41 cache_i (byte tag i)

#ifndef VC_VCASSETREF_H
#define VC_VCASSETREF_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCAssetRef : public VCObject {
public:
    VCAssetRef() noexcept;
    ~VCAssetRef() override;

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
    uint8_t  sub_object[0x10];
    uint32_t cache_f;
    uint32_t cache_g;
    uint8_t  cache_h;
    uint8_t  cache_i;
    uint8_t  _pad_42[2];
};

XFILES_ASSERT_SIZE(VCAssetRef, VCASSETREF_SIZE, "VCAssetRef 0x44");
XFILES_ASSERT_OFFSET(VCAssetRef, sub_object, 0x28, "sub_object");
XFILES_ASSERT_OFFSET(VCAssetRef, cache_f, 0x38, "cache_f");
XFILES_ASSERT_OFFSET(VCAssetRef, cache_g, 0x3C, "cache_g");
XFILES_ASSERT_OFFSET(VCAssetRef, cache_h, 0x40, "cache_h");
XFILES_ASSERT_OFFSET(VCAssetRef, cache_i, 0x41, "cache_i");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCASSETREF_H
