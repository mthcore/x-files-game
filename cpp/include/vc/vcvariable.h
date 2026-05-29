// SPDX-License-Identifier: MIT
// VCVariable - 68B, byte-direct Read decoder.
//   +0x28..+0x37 sub_object (tag e)
//   +0x38 cache_g (uint32, read_version_dword tag g)
//   +0x3C cache_i (uint32, read_dword tag i)
//   +0x40 cache_h (byte tag h)
//   +0x41 cache_f (byte tag f)

#ifndef VC_VCVARIABLE_H
#define VC_VCVARIABLE_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCVariable : public VCObject {
public:
    VCVariable() noexcept;
    ~VCVariable() override;

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
    uint8_t  sub_object[0x10];   // +0x28..+0x37
    uint32_t cache_g;            // +0x38
    uint32_t cache_i;            // +0x3C
    uint8_t  cache_h;            // +0x40
    uint8_t  cache_f;            // +0x41
    uint8_t  _pad_42[2];
};

XFILES_ASSERT_SIZE(VCVariable, VCVARIABLE_SIZE, "VCVariable 0x44");
XFILES_ASSERT_OFFSET(VCVariable, sub_object, 0x28, "sub_object");
XFILES_ASSERT_OFFSET(VCVariable, cache_g, 0x38, "cache_g");
XFILES_ASSERT_OFFSET(VCVariable, cache_i, 0x3C, "cache_i");
XFILES_ASSERT_OFFSET(VCVariable, cache_h, 0x40, "cache_h");
XFILES_ASSERT_OFFSET(VCVariable, cache_f, 0x41, "cache_f");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCVARIABLE_H
