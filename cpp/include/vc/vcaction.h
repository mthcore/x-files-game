// SPDX-License-Identifier: MIT
// VCAction — 108-byte action descriptor.
//
// Byte-direct from the format notes +
// Read :
//   +0x00..+0x03 : vtable ()
//   +0x04..+0x27 : VCObject base scratch (36B)
//   +0x28..+0x63 : sub-collection (56B) — ctor'd by, read by
// with tag 'e' (0x65)
//   +0x64        : byte field, tag 'f' (0x66) — action kind (low 7 bits) +
//                  variant flag (bit 7) ; consumed by post-Read dispatch
// (599-byte switch over kind).
//   +0x65..+0x67 : pad (3B)
//   +0x68..+0x6B : uint32 counter, zeroed in ctor (factory sets obj[0x1a]=0)

#ifndef VC_VCACTION_H
#define VC_VCACTION_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCAction : public VCObject {
public:
    VCAction() noexcept;
    ~VCAction() override;

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

    // +0x04..+0x27 : VCObject base scratch (36B).
    uint8_t  _base_scratch[0x24];

    // +0x28..+0x63 : 56-byte sub-collection (map/list).
    uint8_t  sub_coll[0x3C];

    // +0x64 : action kind byte (low 7 bits = kind, bit 7 = variant flag).
    uint8_t  kind_byte;
    uint8_t  _pad_65[3];

    // +0x68..+0x6B : counter, zeroed at ctor.
    uint32_t counter;
};

XFILES_ASSERT_SIZE(VCAction, VCACTION_SIZE, "VCAction size 0x6c mismatch");
XFILES_ASSERT_OFFSET(VCAction, _base_scratch, 0x04, "_base_scratch offset");
XFILES_ASSERT_OFFSET(VCAction, sub_coll, 0x28, "sub_coll offset");
XFILES_ASSERT_OFFSET(VCAction, kind_byte, 0x64, "kind_byte offset");
XFILES_ASSERT_OFFSET(VCAction, counter, 0x68, "counter offset");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCACTION_H
