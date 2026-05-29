// SPDX-License-Identifier: MIT
// VCNav.cpp - byte-direct Read (slot 12) decoder.

#include "vc/vcnav.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCNav::VCNav() noexcept
    : cache_e(0), cache_f(0), cache_g(0), field_34(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
}

VCNav::~VCNav() = default;

///  :
///   base init + 3 dwords via ctx->vtable[+0x9c]('e'/'f'/'g').
void VCNav::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    cache_e = ctx->reader().read_uint32_field(0x65);
    cache_f = ctx->reader().read_uint32_field(0x66);
    cache_g = ctx->reader().read_uint32_field(0x67);
}

void VCNav::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, cache_e);
    ctx->writer().write_uint32_field(0x66, cache_f);
    ctx->writer().write_uint32_field(0x67, cache_g);
}

}  // namespace vc
}  // namespace xfiles
