// SPDX-License-Identifier: MIT
// VCActionList — class_id 0x42, ordered list of VCAction objects, the
// "scripts" of the game's trigger system.
//
// Source : the on-disk Read sequence 
//          the on-disk Read sequence (list iter)
//          the format notes (size 0x64 = 100 B)
//
// Read body :
// base init
// iter list with tag 'e'
//
// Our reference implementation uses `read_obj_list()` which reads :
//   [NPlt fourcc][count : uint32 LE]
//   N × [NPTc fourcc][class_id : uint32 LE][record bytes]
//
// The list is API-iso : `std::vector<std::unique_ptr<VCObject>>` instead
// of inline storage. This is the **scripts container** — a VCActionList
// is what attaches to VCExplorable/VCLocation/VCCharacter triggers and
// holds the sequence of conditional actions documented in
// `the research datasets`.

#ifndef NL_CLASSES_VC_ACTION_LIST_H
#define NL_CLASSES_VC_ACTION_LIST_H

#include "nl/vc_object.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace xfiles {
namespace nl {

class VCActionList : public VCObject {
public:
    VCActionList();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;

    /// Access the list of items (each item is polymorphic — typically
    /// a VCAction subclass, but any VCObject derivative works).
    const std::vector<std::unique_ptr<VCObject>>& items() const {
        return items_;
    }
    std::size_t size() const { return items_.size(); }
    const VCObject* at(std::size_t i) const {
        return (i < items_.size()) ? items_[i].get() : nullptr;
    }

    static constexpr uint32_t kClassId = 0x42;

private:
    std::vector<std::unique_ptr<VCObject>> items_;
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASSES_VC_ACTION_LIST_H
