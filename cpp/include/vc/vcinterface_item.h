// SPDX-License-Identifier: MIT
// VCInterfaceItem - 84B, byte-direct Read decoder.
//   +0x28..+0x43 sub_object (28B, tag e)
//   +0x44 cache_f (uint32 tag f)
//   +0x48 cache_g (uint32 tag g)
//   +0x4C cache_h (uint16 tag h)
//   +0x50 cache_i (uint32, version-gated tag i)

#ifndef VC_VCINTERFACEITEM_H
#define VC_VCINTERFACEITEM_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCInterfaceItem : public VCObject {
public:
    VCInterfaceItem() noexcept;
    ~VCInterfaceItem() override;

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
    uint8_t  sub_object[0x1C];    // +0x28..+0x43 (28B)
    uint32_t cache_f;             // +0x44
    uint32_t cache_g;             // +0x48
    uint16_t cache_h;             // +0x4C
    uint16_t _pad_4e;
    uint32_t cache_i;             // +0x50 version-gated
};

XFILES_ASSERT_SIZE(VCInterfaceItem, VCINTERFACEITEM_SIZE, "VCInterfaceItem 0x54");
XFILES_ASSERT_OFFSET(VCInterfaceItem, sub_object, 0x28, "sub_object");
XFILES_ASSERT_OFFSET(VCInterfaceItem, cache_f, 0x44, "cache_f");
XFILES_ASSERT_OFFSET(VCInterfaceItem, cache_g, 0x48, "cache_g");
XFILES_ASSERT_OFFSET(VCInterfaceItem, cache_h, 0x4C, "cache_h");
XFILES_ASSERT_OFFSET(VCInterfaceItem, cache_i, 0x50, "cache_i");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCINTERFACEITEM_H
