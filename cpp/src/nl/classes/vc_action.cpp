// SPDX-License-Identifier: MIT
// VCAction byte-direct Read port — 

#include "nl/classes/vc_action.h"
#include "nl/hdb_serializer.h"
#include "nl/vc_factory.h"

namespace xfiles {
namespace nl {

static constexpr uint32_t kFourccNPTc = 0x4e505463u;

VCAction::VCAction() : VCObject(), expr_(), action_type_(0) {}

void VCAction::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;

    // polymorphic Expression
    // sub-object. The reference decoder materializes via read_obj which reads class_id
    // from the stream and constructs the right C++ type.
    expr_ = read_obj(ser, kFourccNPTc);
    if (ser->error()) return;

    // (**(code **)(*param_1 + 0x80))(0x66) — slot 0x80 read_byte with tag
    // 'f' (0x66) — the action_type. Bit 7 (0x80) is a packed flag handled
    // in post-init.
    action_type_ = ser->read_byte(0x66);

    // post-init endianness swap per action_type ;
    // skipped in the reference implementation (we don't carry the in-memory endian
    // sensitivity that the original code preserves for x86 vs PowerPC).
}

void VCAction::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj(ser, kFourccNPTc, expr_.get());
    ser->write_byte(0x66, action_type_);
}

}  // namespace nl
}  // namespace xfiles
