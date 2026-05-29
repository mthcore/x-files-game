// SPDX-License-Identifier: MIT
// VCViewPoint — 124-byte camera viewpoint (scene-graph family).
//
// Byte-direct format layout :
//   +0x00..+0x03 vtable
//   +0x04..+0x27 VCObject base scratch (36B)
//   +0x28..+0x63 sub-collection (60B) —(0x65)
//   +0x64..+0x6F sub-object (12B) — (this+0x64)->vtable[+0x08](ctx, 'f')
//   +0x70 cache_g (uint32 tag 'g')
//   +0x74 cache_h (uint32 tag 'h')
//   +0x78..+0x7B tail field (uninitialised in Read)

#ifndef VC_VCVIEW_POINT_H
#define VC_VCVIEW_POINT_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCViewPoint : public VCObject {
public:
    VCViewPoint() noexcept;
    ~VCViewPoint() override;

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
    uint8_t  sub_object[0x0C];      // +0x64..+0x6F (12-byte CString-like)
    uint32_t cache_g;               // +0x70 tag 'g'
    uint32_t cache_h;               // +0x74 tag 'h'
    uint32_t field_78;              // +0x78 reserved
};

XFILES_ASSERT_SIZE(VCViewPoint, VCVIEWPOINT_SIZE, "VCViewPoint size 0x7c");
XFILES_ASSERT_OFFSET(VCViewPoint, sub_coll, 0x28, "sub_coll");
XFILES_ASSERT_OFFSET(VCViewPoint, sub_object, 0x64, "sub_object");
XFILES_ASSERT_OFFSET(VCViewPoint, cache_g, 0x70, "cache_g");
XFILES_ASSERT_OFFSET(VCViewPoint, cache_h, 0x74, "cache_h");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCVIEW_POINT_H
