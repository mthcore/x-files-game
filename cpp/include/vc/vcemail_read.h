// SPDX-License-Identifier: MIT
// VCEmailRead — byte-exact (identical layout to VCEmail).
//
// Byte-direct size : 0x78 = 120 bytes
// Uses the same custom sub-collection as VCEmail.

#ifndef VC_VCEMAILREAD_H
#define VC_VCEMAILREAD_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"
#include "vc/inline_collection.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCEmailRead : public VCObject {
public:
    VCEmailRead() noexcept;
    ~VCEmailRead() override;

    void vt_slot_01() override {} void vt_slot_02() override {} void vt_slot_03() override {}
    void vt_slot_04() override {} void vt_slot_05() override {}
    char is_leaf() const override { return 0; }
    void vt_slot_07() override {} void vt_slot_08() override {}
    VCObject* id_lookup(uint32_t) override { return nullptr; }
    void vt_slot_10() override {}
    uint32_t eval(uint32_t, uint32_t) override { return 0; }
    void Read(HDBContext* ctx, uint32_t param_2) override;
    void Write(HDBContext* ctx) const override;
    void vt_slot_14() override {} void vt_slot_15() override {} void vt_slot_16() override {}
    void vt_slot_17() override {} void vt_slot_18() override {} void vt_slot_19() override {}
    void vt_slot_20() override {} void vt_slot_21() override {} void vt_slot_22() override {}
    void vt_slot_23() override {} void vt_slot_24() override {} void vt_slot_25() override {}
    void vt_slot_26() override {} void vt_slot_27() override {}

protected:
    char _engine_internal[0x10];
    uint32_t _npfl_flags;
    uint32_t _vers;
    char _intermediate[0x0c];
public:
    InlineCollection sub_coll;     // +0x28
protected:
    char _tail[0x14];              // +0x64..+0x77
};

XFILES_ASSERT_SIZE(VCEmailRead, VCEMAILREAD_SIZE, "VCEmailRead size");
XFILES_ASSERT_OFFSET(VCEmailRead, sub_coll, 0x28, "sub_coll offset");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCEMAILREAD_H
