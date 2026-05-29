// SPDX-License-Identifier: MIT
// VCStdActionList � 100-byte List<T> container (member of the 10-class List family).
//
// Byte-direct format layout :
//   +0x00..+0x03 : vtable
//   +0x04..+0x27 : VCObject base scratch (36B)
//   +0x28..+0x63 : sub-collection (60B), read via the list iterator with tag 'e' (0x65)
//
// All 10 List<T> classes share this exact byte layout. The element type T
// differs per class and is enforced only at construction / element-iteration.

#ifndef VC_VCSTDACTIONLIST_H
#define VC_VCSTDACTIONLIST_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCStdActionList : public VCObject {
public:
    VCStdActionList() noexcept;
    ~VCStdActionList() override;

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

    uint8_t  _base_scratch[0x24];   // +0x04..+0x27 base
    uint8_t  sub_coll[0x3C];        // +0x28..+0x63 60-byte sub-collection
};

XFILES_ASSERT_SIZE(VCStdActionList, VCSTDACTIONLIST_SIZE, "VCStdActionList size 0x64 mismatch");
XFILES_ASSERT_OFFSET(VCStdActionList, _base_scratch, 0x04, "_base_scratch offset");
XFILES_ASSERT_OFFSET(VCStdActionList, sub_coll, 0x28, "sub_coll offset");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCSTDACTIONLIST_H
