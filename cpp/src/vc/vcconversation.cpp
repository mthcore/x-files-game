// SPDX-License-Identifier: MIT
// VCConversation — byte-direct Read/Write per W23.4 TLV grammar.

#include "vc/vcconversation.h"
#include "hdb/hdb_context.h"
#include "hdb/hdb_tlv.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCConversation::VCConversation() noexcept
    : _npfl_flags(0), _vers(5),
      field_e(0), field_f(0), field_g(0), field_h(0), field_i(0), field_j(0) {
    std::memset(_engine_internal, 0, sizeof(_engine_internal));
    std::memset(_intermediate, 0, sizeof(_intermediate));
    std::memset(_tail, 0, sizeof(_tail));
}

VCConversation::~VCConversation() = default;

void VCConversation::Read(HDBContext* ctx, uint32_t /*param_2*/) {
    if (!ctx) return;
    ctx->read_base_init(_npfl_flags, _vers);
    auto& r = ctx->reader();
    // Read order from W23.4 decoded body : 'e' 'f' 'h' 'j' 'g' 'i'
    field_e = r.read_uint32_field(0x65);
    field_f = r.read_uint32_field(0x66);
    field_h = r.read_uint32_field(0x68);
    field_j = r.read_uint32_field(0x6a);
    field_g = r.read_uint32_field(0x67);
    field_i = r.read_uint32_field(0x69);
}

void VCConversation::Write(HDBContext* ctx) const {
    if (!ctx) return;
    ctx->write_base_init(_npfl_flags, _vers);
    auto& w = ctx->writer();
    // Write order : same as read (preserves byte-identique round-trip)
    w.write_uint32_field(0x65, field_e);
    w.write_uint32_field(0x66, field_f);
    w.write_uint32_field(0x68, field_h);
    w.write_uint32_field(0x6a, field_j);
    w.write_uint32_field(0x67, field_g);
    w.write_uint32_field(0x69, field_i);
}

}  // namespace vc
}  // namespace xfiles
