// SPDX-License-Identifier: MIT
// VCCursor.cpp — byte-direct Read (slot 12) decoder.

#include "vc/vccursor.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCCursor::VCCursor() noexcept : cache_f(0), cache_g(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(sprite_string, 0, sizeof(sprite_string));
}

VCCursor::~VCCursor() = default;

/// Byte-direct Read decoder (vtable slot 12) :
/// base init
/// >vtable[+0x08])(this+0x28, ctx, 0x65);  // sub_obj Read 'e'
void VCCursor::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    // CString sub-object Read at this+0x28 with tag 'e' (0x65). In the
    // in the reference implementation the CString is a 12-byte placeholder ; we just consume the TLV
    // record to keep the reader cursor aligned.
    (void)ctx->reader().read_uint32_field(0x65);
    cache_f = ctx->reader().read_uint32_field(0x66);
    cache_g = ctx->reader().read_uint32_field(0x67);
}

void VCCursor::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, 0);
    ctx->writer().write_uint32_field(0x66, cache_f);
    ctx->writer().write_uint32_field(0x67, cache_g);
}

}  // namespace vc
}  // namespace xfiles
