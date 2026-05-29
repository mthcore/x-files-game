// SPDX-License-Identifier: MIT
// VCDefaultCursors - 72B. Byte-direct Read decoder (Read slot 12).
//
// Body : iterates a *compact* sub-collection at +0x28 (NOT the standard 60-byte
// InlineCollection — the variant here fits in 32 bytes). Each item is a sprite
// ID for a cursor kind, read via sequential tag bytes starting at 0x65.
//
// Layout :
//   +0x00..+0x03 vtable
//   +0x04..+0x27 VCObject base scratch (36B)
//   +0x28..+0x47 compact sub-coll (32B, opaque vtable + items array)
//
// Read iteration count is managed via a side-table indexed by `this` (see
// VCDefaultCursors::set_item_count() / item_count() / set_item_id() /
// get_item_id()). The side-table side-steps the 32B layout limit.

#ifndef VC_VCDEFAULTCURSORS_H
#define VC_VCDEFAULTCURSORS_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCDefaultCursors : public VCObject {
public:
    VCDefaultCursors() noexcept;
    ~VCDefaultCursors() override;

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

    // Side-table helpers : let a future revision / tests populate the items list.
    void set_item_count(std::size_t n);
    std::size_t item_count() const;
    uint32_t get_item_id(std::size_t idx) const;

    uint8_t  _base_scratch[0x24];   // +0x04..+0x27 base
    uint8_t  sub_coll_compact[32];  // +0x28..+0x47 compact 32B sub-coll
};

XFILES_ASSERT_SIZE(VCDefaultCursors, VCDEFAULTCURSORS_SIZE, "VCDefaultCursors 0x48");
XFILES_ASSERT_OFFSET(VCDefaultCursors, _base_scratch, 0x04, "_base_scratch");
XFILES_ASSERT_OFFSET(VCDefaultCursors, sub_coll_compact, 0x28, "sub_coll_compact");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCDEFAULTCURSORS_H
