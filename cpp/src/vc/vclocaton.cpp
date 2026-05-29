// SPDX-License-Identifier: MIT
// VCLocaton.cpp — byte-direct Read (slot 12) decoder.

#include "vc/vclocaton.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCLocaton::VCLocaton() noexcept
    : cache_f(0), cache_g(0), cache_h(0), field_70(0), cache_i(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(sub_coll,      0, sizeof(sub_coll));
    std::memset(_pad_75,       0, sizeof(_pad_75));
}

VCLocaton::~VCLocaton() = default;

void VCLocaton::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    (void)ctx->reader().read_uint32_field(0x65);  // sub-coll
    cache_f = ctx->reader().read_uint32_field(0x66);
    cache_g = ctx->reader().read_uint32_field(0x67);
    cache_h = ctx->reader().read_uint32_field(0x68);
    cache_i = static_cast<uint8_t>(
        ctx->reader().read_uint32_field(0x69) & 0xff);
}

void VCLocaton::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, 0);
    ctx->writer().write_uint32_field(0x66, cache_f);
    ctx->writer().write_uint32_field(0x67, cache_g);
    ctx->writer().write_uint32_field(0x68, cache_h);
    ctx->writer().write_uint32_field(0x69, static_cast<uint32_t>(cache_i));
}

}  // namespace vc
}  // namespace xfiles
