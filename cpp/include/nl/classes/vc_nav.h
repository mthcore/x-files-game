// SPDX-License-Identifier: MIT
// VCNav — Navigation hotspot, class_id 0x33 (51 dec), sizeof 0x38 (56 B).
//
// Source : the on-disk Read sequence 
//          the format notes (class_id 0x33)
//          the format notes (size 0x38 = 56 B)
//
// Used for in-scene navigation hotspots (move to adjacent ViewPoint).
// One of the most-used VC classes in gameplay : every navigable scene has
// 4-8 VCNav records for N/S/E/W directions + close-ups.
//
// Layout :
//   +0x00..0x27 : VCObjectBase (40 B)
//   +0x28 'e'   uint32  (target view id or asset id)
//   +0x2c 'f'   uint32  (flag bits)
//   +0x30 'g'   uint32  (probably : direction index or behavior)
//   +0x34..0x37 : padding (4 B trailing pad to reach class size 0x38)
//
// Read (, 24 lines) :
//   base init + 3 direct read_dword(e/f/g). No sub-object, no
//   version-gating. Trivially portable byte-direct.

#ifndef NL_CLASSES_VC_NAV_H
#define NL_CLASSES_VC_NAV_H

#include "nl/vc_object.h"

#include <cstdint>

namespace xfiles {
namespace nl {

struct VCNavFields {
    uint32_t e_at_0x28;   // probable target view id / asset id
    uint32_t f_at_0x2c;   // probable flag bits
    uint32_t g_at_0x30;   // probable direction / behavior
    uint32_t pad_at_0x34; // trailing pad to reach sizeof(class) = 0x38
};

class VCNav : public VCObject {
public:
    VCNav();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;

    const VCNavFields& fields() const { return fields_; }
    VCNavFields&       fields()       { return fields_; }

    static constexpr uint32_t kClassId = 0x33;
    static constexpr std::size_t kSizeOf = 0x38;

private:
    VCNavFields fields_;
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASSES_VC_NAV_H
