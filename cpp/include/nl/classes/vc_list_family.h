// SPDX-License-Identifier: MIT
// VCList family — 10 polymorphic list classes sharing the same Read
// pattern : base init + iterate list via the list iterator with tag 'e'.
//
// Source : VCAssetRefList::Read, VCActionList::Read
//          + identical bodies for VCCharViewList, VCConversationList,
//          VCNav_List, VCExplorableList, VCHotSpotList,
//          VCStdHotSpotList, VCIdeaMapList, VCTriggerList,
//          VCStdActionList.
//
// All share : `base init + list iter (ser, 0x65)`. Our reference implementation uses
// `read_obj_list` for the polymorphic iteration. Items are stored in
// `std::vector<std::unique_ptr<VCObject>>` (API-iso).
//
// VCActionList (class_id 0x42) is already in vc_action_list.h ; the
// other 10 lists live here.

#ifndef NL_CLASSES_VC_LIST_FAMILY_H
#define NL_CLASSES_VC_LIST_FAMILY_H

#include "nl/vc_object.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace xfiles {
namespace nl {

/// Shared list Read : VCObject::Read + read_obj_list with the standard
/// markers (NPlt count + NPTc class_id). Returns the vector of items
/// (caller stores in its member).
std::vector<std::unique_ptr<VCObject>> read_list_family_items(
        HDBSerializer* ser);

/// Mirror Write.
void write_list_family_items(
        HDBSerializer* ser,
        const std::vector<std::unique_ptr<VCObject>>& items);

#define DECL_LIST_FAMILY_CLASS(name, class_id_val)                    \
    class name : public VCObject {                                     \
    public:                                                             \
        name() : VCObject(), items_() {}                               \
        void Read(HDBSerializer* ser) override {                       \
            VCObject::Read(ser);                                        \
            if (!ser || ser->error()) return;                          \
            items_ = read_list_family_items(ser);                       \
        }                                                               \
        void Write(HDBSerializer* ser) const override {                 \
            VCObject::Write(ser);                                        \
            write_list_family_items(ser, items_);                       \
        }                                                               \
        const std::vector<std::unique_ptr<VCObject>>& items() const {  \
            return items_;                                              \
        }                                                               \
        std::size_t size() const { return items_.size(); }             \
        const VCObject* at(std::size_t i) const {                       \
            return (i < items_.size()) ? items_[i].get() : nullptr;    \
        }                                                               \
        static constexpr uint32_t kClassId = class_id_val;             \
    private:                                                            \
        std::vector<std::unique_ptr<VCObject>> items_;                  \
    }

DECL_LIST_FAMILY_CLASS(VCCharViewList,      0x2e);
DECL_LIST_FAMILY_CLASS(VCConversationList,  0x32);
DECL_LIST_FAMILY_CLASS(VCNav_List,          0x34);
DECL_LIST_FAMILY_CLASS(VCAssetRefList,      0x36);
DECL_LIST_FAMILY_CLASS(VCExplorableList,    0x38);
DECL_LIST_FAMILY_CLASS(VCHotSpotList,       0x3a);
DECL_LIST_FAMILY_CLASS(VCStdHotSpotList,    0x3b);
DECL_LIST_FAMILY_CLASS(VCIdeaMapList,       0x4d);
DECL_LIST_FAMILY_CLASS(VCTriggerList,       0x52);
DECL_LIST_FAMILY_CLASS(VCStdActionList,     0x55);

#undef DECL_LIST_FAMILY_CLASS

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASSES_VC_LIST_FAMILY_H
