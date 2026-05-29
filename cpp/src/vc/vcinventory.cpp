// SPDX-License-Identifier: MIT
// VCInventory.cpp — byte-direct Read/Write per W23.4 TLV grammar.

#include "vc/vcinventory.h"
#include "hdb/hdb_context.h"
#include "hdb/hdb_tlv.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCInventory::VCInventory() noexcept
    : _npfl_flags(0), _vers(5),
      field_e(0), field_f(0), field_g(0), field_h(0),
      field_k(0), field_i(0), field_j(0) {
    std::memset(_engine_internal_0, 0, sizeof(_engine_internal_0));
    std::memset(_intermediate_0, 0, sizeof(_intermediate_0));
}

VCInventory::~VCInventory() = default;

void VCInventory::Read(HDBContext* ctx, uint32_t /*param_2*/) {
    if (!ctx) return;
    // Base init : NPfl flag byte + vers version dword ( equivalent).
    ctx->read_base_init(_npfl_flags, _vers);
    // 7 uint32 fields, tags 'e'..'k' (skip 'k' if vers <= 3) per W23.4 byte-direct.
    auto& r = ctx->reader();
    field_e = r.read_uint32_field(0x65);  // 'e'
    field_f = r.read_uint32_field(0x66);  // 'f'
    field_g = r.read_uint32_field(0x67);  // 'g'
    field_h = r.read_uint32_field(0x68);  // 'h'
    field_i = r.read_uint32_field(0x69);  // 'i'
    field_j = r.read_uint32_field(0x6a);  // 'j'
    if (_vers > 3) {
        field_k = r.read_uint32_field(0x6b);  // 'k' (versioned)
    }
}

void VCInventory::Write(HDBContext* ctx) const {
    if (!ctx) return;
    ctx->write_base_init(_npfl_flags, _vers);
    auto& w = ctx->writer();
    w.write_uint32_field(0x65, field_e);
    w.write_uint32_field(0x66, field_f);
    w.write_uint32_field(0x67, field_g);
    w.write_uint32_field(0x68, field_h);
    w.write_uint32_field(0x69, field_i);
    w.write_uint32_field(0x6a, field_j);
    if (_vers > 3) {
        w.write_uint32_field(0x6b, field_k);
    }
}

}  // namespace vc
}  // namespace xfiles
