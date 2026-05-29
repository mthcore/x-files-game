// SPDX-License-Identifier: MIT
// VCString - byte-direct Read (slot 12) decoder.

#ifndef VC_VCSTRING_H
#define VC_VCSTRING_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCString : public VCObject {
public:
    VCString() noexcept;
    ~VCString() override;

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
    uint8_t  sub_object[0x10];  // +0x28..+0x37 (16-byte CString-like)

};

XFILES_ASSERT_SIZE(VCString, VCSTRING_SIZE, "VCString size 0x38");
XFILES_ASSERT_OFFSET(VCString, sub_object, 0x28, "sub_object");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCSTRING_H
