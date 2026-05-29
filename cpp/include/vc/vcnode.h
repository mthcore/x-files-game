// SPDX-License-Identifier: MIT
// VCNode — 112-byte generic scene/logic-graph node container.
//
// Byte-direct format layout :
//   +0x00..+0x03 : vtable
//   +0x04..+0x27 : VCObject base scratch (36B)
//   +0x28..+0x3F : sub-object (24B) — CString-like, ClassMeta init, type=0xb,
//                  read via the sub-object reader with tag 'e' (0x65)
//   +0x40..+0x5F : payload (32B) — children/array slot, TBD semantics
//   +0x60..+0x63 : uint32 = 0 (set in ctor)
//   +0x64..+0x67 : uint32 field, tag 'f' (0x66)
//   +0x68..+0x6B : uint32 field, tag 'g' (0x67)
//   +0x6C..+0x6F : uint32 = 0 (puVar1[0x1b] in ctor)

#ifndef VC_VCNODE_H
#define VC_VCNODE_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCNode : public VCObject {
public:
    VCNode() noexcept;
    ~VCNode() override;

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

    uint8_t  _base_scratch[0x24];   // +0x04..+0x27 VCObject base
    uint8_t  sub_object[0x18];      // +0x28..+0x3F : CString-like sub-object (24B)
    uint8_t  payload[0x20];         // +0x40..+0x5F : children/array (32B)
    uint32_t field_60;              // +0x60 : zeroed in ctor
    uint32_t cache_f;               // +0x64 : tag 'f' (0x66)
    uint32_t cache_g;               // +0x68 : tag 'g' (0x67)
    uint32_t field_6c;              // +0x6c : zeroed in ctor
};

XFILES_ASSERT_SIZE(VCNode, VCNODE_SIZE, "VCNode size 0x70 mismatch");
XFILES_ASSERT_OFFSET(VCNode, _base_scratch, 0x04, "_base_scratch offset");
XFILES_ASSERT_OFFSET(VCNode, sub_object, 0x28, "sub_object offset");
XFILES_ASSERT_OFFSET(VCNode, payload, 0x40, "payload offset");
XFILES_ASSERT_OFFSET(VCNode, field_60, 0x60, "field_60 offset");
XFILES_ASSERT_OFFSET(VCNode, cache_f, 0x64, "cache_f offset");
XFILES_ASSERT_OFFSET(VCNode, cache_g, 0x68, "cache_g offset");
XFILES_ASSERT_OFFSET(VCNode, field_6c, 0x6C, "field_6c offset");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCNODE_H
