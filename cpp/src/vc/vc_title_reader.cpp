// SPDX-License-Identifier: MIT
// VCTitle byte-direct deserializer implementation.

#include "vc/vc_title_reader.h"

namespace xfiles {
namespace vc {

bool read_vc_title(hdb::TLVReader& reader, VCTitleData* out) {
    if (!out) return false;
    *out = {};
    // prelude : skip NPfl + vers headers.
    reader.prelude();
    // 4 sub-CStrings (the on-disk format : vtable[+0x8] dispatch on a sub-CString object).
    // We read them as plain strings since our TLVReader treats them as
    // length-prefixed payloads.
    out->string_f = reader.read_string('f');
    out->string_g = reader.read_string('g');
    out->string_h = reader.read_string('h');
    out->string_i = reader.read_string('i');
    // 2 uint32 fields (tags 'j', 'k').
    out->field_9c = reader.read_uint32('j');
    out->field_a0 = reader.read_uint32('k');
    // 2 uint16 fields (tags 'l', 'm').
    out->field_a4 = reader.read_uint16('l');
    out->field_a6 = reader.read_uint16('m');
    // Success if at least one tag was hit (i.e., not every read missed).
    return !reader.any_miss()
        || !out->string_f.empty() || !out->string_g.empty()
        || !out->string_h.empty() || !out->string_i.empty()
        || out->field_9c != 0 || out->field_a0 != 0
        || out->field_a4 != 0 || out->field_a6 != 0;
}

}  // namespace vc
}  // namespace xfiles
