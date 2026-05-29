// SPDX-License-Identifier: MIT
// VCAction — class_id 0x41, sizeof 0x6c (108 B). The action body of a
// trigger : a polymorphic Expression (condition) at +0x28 tagged 'e' and
// a byte action_type at +0x64 tagged 'f'.
//
// Source : the on-disk Read sequence 
//          the on-disk Read sequence (post-init, endian
//                                          swap per action_type — skipped
//                                          in our reference implementation, API-iso)
//          the format notes (size 0x6c)
//
// Read body :
// base init
// sub-object 'e' (expression)
//   this+0x64 = ((**param_1->vt + 0x80))(0x66);     // byte 'f' (action_type)
// post-init endian swap
//
// The Expression sub-object encodes the trigger condition (LHS op RHS) ;
// the action_type byte (0..7) maps to the 8 ActionTypes documented in
// `the research datasets` :
//   0 Statement, 1 Asset, 2 Timer, 3 Enable, 4 SetView, 5 Interface,
//   6 C++ Function (UDF), 7 3D Sound.

#ifndef NL_CLASSES_VC_ACTION_H
#define NL_CLASSES_VC_ACTION_H

#include "nl/vc_object.h"

#include <cstdint>
#include <memory>

namespace xfiles {
namespace nl {

class VCAction : public VCObject {
public:
    VCAction();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;

    /// Polymorphic Expression sub-object at on-disk offset +0x28 (the
    /// condition predicate of this action). Null if not present in stream.
    const VCObject* expression() const { return expr_.get(); }
    VCObject*       expression()       { return expr_.get(); }

    /// Action type byte at on-disk offset +0x64. Low 3 bits = ActionType
    /// enum (0..7) ; bit 7 (0x80) is a packed flag (cf.).
    uint8_t action_type_raw() const { return action_type_; }
    uint8_t action_type() const { return action_type_ & 0x7f; }

    static constexpr uint32_t kClassId = 0x41;
    static constexpr std::size_t kNativeSizeOf = 0x6c;

private:
    std::unique_ptr<VCObject> expr_;
    uint8_t                   action_type_ = 0;
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASSES_VC_ACTION_H
