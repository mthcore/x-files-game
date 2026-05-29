// SPDX-License-Identifier: MIT
// VCDefaultHotSpots - 72B. Byte-direct Read decoder (Read slot 12).
// Same shape as VCDefaultCursors : iterates compact sub-coll at +0x28 reading
// sequential tag bytes from 0x65. Items live in side-table keyed by `this`.

#ifndef VC_VCDEFAULTHOTSPOTS_H
#define VC_VCDEFAULTHOTSPOTS_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCDefaultHotSpots : public VCObject {
public:
    VCDefaultHotSpots() noexcept;
    ~VCDefaultHotSpots() override;

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

    void set_item_count(std::size_t n);
    std::size_t item_count() const;
    uint32_t get_item_id(std::size_t idx) const;

    uint8_t  _base_scratch[0x24];
    uint8_t  sub_coll_compact[32];
};

XFILES_ASSERT_SIZE(VCDefaultHotSpots, VCDEFAULTHOTSPOTS_SIZE, "VCDefaultHotSpots 0x48");
XFILES_ASSERT_OFFSET(VCDefaultHotSpots, _base_scratch, 0x04, "_base_scratch");
XFILES_ASSERT_OFFSET(VCDefaultHotSpots, sub_coll_compact, 0x28, "sub_coll_compact");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCDEFAULTHOTSPOTS_H
