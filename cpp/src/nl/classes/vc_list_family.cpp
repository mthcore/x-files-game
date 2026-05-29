// SPDX-License-Identifier: MIT
// List family shared reader.

#include "nl/classes/vc_list_family.h"
#include "nl/hdb_serializer.h"
#include "nl/vc_factory.h"

namespace xfiles {
namespace nl {

static constexpr uint32_t kFourccNPlt = 0x4e506c74u;  // 'NPlt'
static constexpr uint32_t kFourccNPTc = 0x4e505463u;  // 'NPTc'

std::vector<std::unique_ptr<VCObject>> read_list_family_items(
        HDBSerializer* ser) {
    if (!ser || ser->error()) return {};
    return read_obj_list(ser, kFourccNPlt, kFourccNPTc);
}

void write_list_family_items(
        HDBSerializer* ser,
        const std::vector<std::unique_ptr<VCObject>>& items) {
    if (!ser) return;
    write_obj_list(ser, kFourccNPlt, kFourccNPTc, items);
}

}  // namespace nl
}  // namespace xfiles
