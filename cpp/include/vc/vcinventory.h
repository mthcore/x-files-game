// SPDX-License-Identifier: MIT
// VCInventory — byte-exact member layout (P2 + P3 ready).
//
// Byte-direct size : 0x44 = 68 bytes
// Read thunks to the shared Icon-family reader.
//
// TLV grammar (byte-direct) :
//   +0x28 tag 'e' (0x65) : uint32   — field_e
//   +0x2c tag 'f' (0x66) : uint32   — field_f
//   +0x30 tag 'g' (0x67) : uint32   — field_g
//   +0x34 tag 'h' (0x68) : uint32   — field_h
//   +0x38 tag 'k' (0x6b) : uint32   — field_k (only if vers > 3)
//   +0x3c tag 'i' (0x69) : uint32   — field_i
//   +0x40 tag 'j' (0x6a) : uint32   — field_j
//
// Base init via //   +0x14 : NPfl flag byte
//   +0x18 : vers version dword (currently 5)
//
// Reference : the format notes
//             the on-disk Read sequence
//             notes/W22_1_FINDINGS.md (30 VCInventory allocs runtime confirmed)

#ifndef VC_VCINVENTORY_H
#define VC_VCINVENTORY_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCInventory : public VCObject {
public:
    VCInventory() noexcept;
    ~VCInventory() override;

    // P2-P3 placeholder overrides (Read/Write will be implemented in P3)
    void vt_slot_01() override {}
    void vt_slot_02() override {}
    void vt_slot_03() override {}
    void vt_slot_04() override {}
    void vt_slot_05() override {}
    char is_leaf() const override { return 1; }  // leaf object (W23.4 mechanism)
    void vt_slot_07() override {}
    void vt_slot_08() override {}
    VCObject* id_lookup(uint32_t) override { return nullptr; }
    void vt_slot_10() override {}
    uint32_t eval(uint32_t, uint32_t) override { return 0; }
    void Read(HDBContext* ctx, uint32_t param_2) override;
    void Write(HDBContext* ctx) const override;
    void vt_slot_14() override {}
    void vt_slot_15() override {}
    void vt_slot_16() override {}
    void vt_slot_17() override {}
    void vt_slot_18() override {}
    void vt_slot_19() override {}
    void vt_slot_20() override {}
    void vt_slot_21() override {}
    void vt_slot_22() override {}
    void vt_slot_23() override {}
    void vt_slot_24() override {}
    void vt_slot_25() override {}
    void vt_slot_26() override {}
    void vt_slot_27() override {}

private:
    // +0x04..+0x13 : engine internal (12 bytes) — TBD
    char _engine_internal_0[0x10];
    // +0x14 : NPfl base flags
    uint32_t _npfl_flags;
    // +0x18 : vers version dword
    uint32_t _vers;
    // +0x1c..+0x27 : intermediate base class data (12 bytes) — TBD
    char _intermediate_0[0x0c];

public:
    // +0x28..+0x40 : byte-exact TLV fields (W23.4)
    uint32_t field_e;  // +0x28 tag 'e'
    uint32_t field_f;  // +0x2c tag 'f'
    uint32_t field_g;  // +0x30 tag 'g'
    uint32_t field_h;  // +0x34 tag 'h'
    uint32_t field_k;  // +0x38 tag 'k' (vers > 3 only)
    uint32_t field_i;  // +0x3c tag 'i'
    uint32_t field_j;  // +0x40 tag 'j'
};

// P2 gate : size enforcement
XFILES_ASSERT_SIZE(VCInventory, VCINVENTORY_SIZE, "VCInventory byte-direct size mismatch");

// P2 gate : member offset enforcement (byte-direct from W23.4)
XFILES_ASSERT_OFFSET(VCInventory, field_e, 0x28, "field_e offset mismatch");
XFILES_ASSERT_OFFSET(VCInventory, field_f, 0x2c, "field_f offset mismatch");
XFILES_ASSERT_OFFSET(VCInventory, field_g, 0x30, "field_g offset mismatch");
XFILES_ASSERT_OFFSET(VCInventory, field_h, 0x34, "field_h offset mismatch");
XFILES_ASSERT_OFFSET(VCInventory, field_k, 0x38, "field_k offset mismatch");
XFILES_ASSERT_OFFSET(VCInventory, field_i, 0x3c, "field_i offset mismatch");
XFILES_ASSERT_OFFSET(VCInventory, field_j, 0x40, "field_j offset mismatch");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCINVENTORY_H
