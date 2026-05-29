// SPDX-License-Identifier: MIT
// VCActionList Read — decoder.

#include "nl/classes/vc_action_list.h"
#include "nl/hdb_serializer.h"
#include "nl/vc_factory.h"

namespace xfiles {
namespace nl {

static constexpr uint32_t kFourccNPlt = 0x4e506c74u;  // 'NPlt' — list count
static constexpr uint32_t kFourccNPTc = 0x4e505463u;  // 'NPTc' — class_id

VCActionList::VCActionList() : VCObject(), items_() {}

void VCActionList::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;

    // iterate the list. The reference decoder uses our
    // polymorphic list reader with the same wire markers.
    items_ = read_obj_list(ser, kFourccNPlt, kFourccNPTc);
}

void VCActionList::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj_list(ser, kFourccNPlt, kFourccNPTc, items_);
}

}  // namespace nl
}  // namespace xfiles
