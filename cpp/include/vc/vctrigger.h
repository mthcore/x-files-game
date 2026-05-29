// SPDX-License-Identifier: MIT
// VCTrigger — 236-byte trigger with 3 inline sub-collections.
//
// Byte-direct size : 0xec = 236 bytes
// Read : vtable slot 12 (an "Attach" pattern).
// Class id         : 0x51
//
// Byte layout per the format notes (W5 + factory read) :
//   +0x00..+0x03 : vtable ()
//   +0x04..+0x27 : VCObject base scratch (36B)
//   +0x28..+0x5B : Sub-collection A (52B, capacity 8)  — Conditions
//   +0x5C..+0x9F : Sub-collection B (68B, capacity 12) — Actions
//   +0xA0..+0xE3 : Sub-collection C (68B, capacity 4)  — Targets
//   +0xE4..+0xE7 : int field cached from ctx, tag 'e' (0x65)
//   +0xE8        : byte field cached from ctx, tag 'f' (0x66)
//   +0xE9..+0xEB : pad (3B)
//
// The Read method ONLY reads NPfl/vers base + the two scalar
// fields at +0xE4 and +0xE8. The 3 inline collections are populated via
// vtable-method calls on the sub-collection objects themselves (Attach
// pattern), not by this Read.

#ifndef VC_VCTRIGGER_H
#define VC_VCTRIGGER_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCTrigger : public VCObject {
public:
    VCTrigger() noexcept;
    ~VCTrigger() override;

    void vt_slot_01() override {} void vt_slot_02() override {} void vt_slot_03() override {}
    void vt_slot_04() override {} void vt_slot_05() override {}
    char is_leaf() const override { return 0; }
    void vt_slot_07() override {} void vt_slot_08() override {}
    VCObject* id_lookup(uint32_t) override { return nullptr; }
    void vt_slot_10() override {}
    uint32_t eval(uint32_t arg1, uint32_t arg2) override;       // slot 11 — trigger Eval
    void Read(HDBContext*, uint32_t) override;                  // slot 12
    void Write(HDBContext*) const override;
    void vt_slot_14() override {} void vt_slot_15() override {} void vt_slot_16() override {}
    void vt_slot_17() override {} void vt_slot_18() override {} void vt_slot_19() override {}
    void vt_slot_20() override {} void vt_slot_21() override {} void vt_slot_22() override {}
    void vt_slot_23() override {} void vt_slot_24() override {} void vt_slot_25() override {}
    void vt_slot_26() override {} void vt_slot_27() override {}

    // +0x04..+0x27 : VCObject base scratch (36B incl. flag bytes set by Read).
    uint8_t  _base_scratch[0x24];

    // +0x28..+0x5B : Sub-collection A (52B = 13 dwords) — Conditions, capacity 8.
    uint8_t  subcoll_conditions[0x34];

    // +0x5C..+0x9F : Sub-collection B (68B = 17 dwords) — Actions, capacity 12.
    uint8_t  subcoll_actions[0x44];

    // +0xA0..+0xE3 : Sub-collection C (68B = 17 dwords) — Targets, capacity 4.
    uint8_t  subcoll_targets[0x44];

    // +0xE4 : 32-bit eval cache, populated from ctx tag 'e' (0x65) at Read time.
    uint32_t cache_e;

    // +0xE8 : 8-bit flag cache, populated from ctx tag 'f' (0x66) at Read time.
    uint8_t  cache_f;
    uint8_t  _pad_e9[3];
};

XFILES_ASSERT_SIZE(VCTrigger, VCTRIGGER_SIZE, "VCTrigger size 0xec mismatch");
XFILES_ASSERT_OFFSET(VCTrigger, _base_scratch, 0x04, "base scratch offset");
XFILES_ASSERT_OFFSET(VCTrigger, subcoll_conditions, 0x28, "conditions sub-coll offset");
XFILES_ASSERT_OFFSET(VCTrigger, subcoll_actions, 0x5C, "actions sub-coll offset");
XFILES_ASSERT_OFFSET(VCTrigger, subcoll_targets, 0xA0, "targets sub-coll offset");
XFILES_ASSERT_OFFSET(VCTrigger, cache_e, 0xE4, "cache_e offset");
XFILES_ASSERT_OFFSET(VCTrigger, cache_f, 0xE8, "cache_f offset");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCTRIGGER_H
