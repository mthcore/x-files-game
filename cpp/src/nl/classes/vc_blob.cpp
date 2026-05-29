// SPDX-License-Identifier: MIT
// VCBlob Read — byte-direct semantics, API-iso storage.

#include "nl/classes/vc_blob.h"
#include "nl/vc_factory.h"

namespace xfiles {
namespace nl {

// 'NPTc' marker preceding the sub-object's class_id in the stream.
// Matches the on-disk fourcc used by slot 0x138 (read_obj).
static constexpr uint32_t kFourccNPTc = 0x4e505463u;

VCBlob::VCBlob() : VCObject(), sub_e_() {}

void VCBlob::Read(HDBSerializer* ser) {
    VCObject::Read(ser);  // base init : NPfl + vers + flags + refcount
    if (!ser || ser->error()) return;

    // polymorphic sub-object
    // read. Our reference implementation materializes the sub-object via read_obj which
    // reads a class_id from the stream and dispatches to the right type.
    sub_e_ = read_obj(ser, kFourccNPTc);
}

void VCBlob::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj(ser, kFourccNPTc, sub_e_.get());
}

}  // namespace nl
}  // namespace xfiles
