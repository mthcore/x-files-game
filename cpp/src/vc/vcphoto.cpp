// SPDX-License-Identifier: MIT
// VCPhoto — byte-exact Read/Write per W23.4 TLV grammar.

#include "vc/vcphoto.h"
#include "hdb/hdb_context.h"
#include "hdb/hdb_tlv.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCPhoto::VCPhoto() noexcept
    : _npfl_flags(0), _vers(5),
      field_i(0), field_j(0) {
    std::memset(_engine_internal, 0, sizeof(_engine_internal));
    std::memset(_intermediate, 0, sizeof(_intermediate));
    std::memset(_tail0, 0, sizeof(_tail0));
    std::memset(_tail1, 0, sizeof(_tail1));
    sub_coll.parent_ptr = this;
}

VCPhoto::~VCPhoto() = default;

void VCPhoto::Read(HDBContext* ctx, uint32_t /*param_2*/) {
    if (!ctx) return;
    // base init + sub-coll tag 'e' + 2 direct byte fields i/j.
    ctx->read_base_init(_npfl_flags, _vers);
    sub_coll.read_header(ctx, 0x65);  // 'e'
    auto& r = ctx->reader();
    // Direct byte reads — encoded as uint32 in TLV but we keep low byte only.
    field_i = static_cast<uint8_t>(r.read_uint32_field(0x69));  // 'i'
    field_j = static_cast<uint8_t>(r.read_uint32_field(0x6a));  // 'j'
}

void VCPhoto::Write(HDBContext* ctx) const {
    if (!ctx) return;
    ctx->write_base_init(_npfl_flags, _vers);
    sub_coll.write_header(ctx, 0x65);
    auto& w = ctx->writer();
    w.write_uint32_field(0x69, field_i);
    w.write_uint32_field(0x6a, field_j);
}

}  // namespace vc
}  // namespace xfiles
