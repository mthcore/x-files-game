// SPDX-License-Identifier: MIT
// VCEmail family shared sub-reader.

#include "nl/classes/vc_email_family.h"
#include "nl/hdb_serializer.h"
#include "nl/vc_factory.h"

namespace xfiles {
namespace nl {

static constexpr uint32_t kFourccNPTc = 0x4e505463u;

std::unique_ptr<VCObject> read_email_family_sub(HDBSerializer* ser) {
    if (!ser || ser->error()) return nullptr;
    // polymorphic sub-obj 'f'
    return read_obj(ser, kFourccNPTc);
}

void write_email_family_sub(HDBSerializer* ser, const VCObject* sub) {
    if (!ser) return;
    write_obj(ser, kFourccNPTc, sub);
}

}  // namespace nl
}  // namespace xfiles
