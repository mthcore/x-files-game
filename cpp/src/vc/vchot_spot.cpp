// SPDX-License-Identifier: MIT
// VCHotSpot.cpp - byte-direct Read (slot 12) decoder.

#include "vc/vchot_spot.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCHotSpot::VCHotSpot() noexcept : cache_f(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(sub_object,    0, sizeof(sub_object));
    std::memset(reserved_38,   0, sizeof(reserved_38));
}

VCHotSpot::~VCHotSpot() = default;

/// ///(ctx, mode)
/// >vtable[+0x08])(this+0x28, ctx, 0x65)  // sub-obj Read 'e'
///   cache_f = ctx->vtable[+0x9c](0x66)                    // uint32 'f'
void VCHotSpot::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    (void)ctx->reader().read_uint32_field(0x65);  // sub_object
    cache_f = ctx->reader().read_uint32_field(0x66);
}

void VCHotSpot::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, 0);
    ctx->writer().write_uint32_field(0x66, cache_f);
}

}  // namespace vc
}  // namespace xfiles
