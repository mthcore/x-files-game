// SPDX-License-Identifier: MIT
// VCEnabled.cpp - byte-direct Read decoder.

#include "vc/vcenabled.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCEnabled::VCEnabled() noexcept : cache_e(0), cache_g(0), cache_f(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(_pad_2e, 0, sizeof(_pad_2e));
}

VCEnabled::~VCEnabled() = default;

void VCEnabled::Read(hdb::HDBContext* ctx, uint32_t) {
    if (!ctx) return;
    uint32_t npfl=0, vers=0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    cache_e = ctx->reader().read_uint32_field(0x65);
    cache_f = static_cast<uint8_t>(ctx->reader().read_uint32_field(0x66) & 0xff);
    cache_g = static_cast<uint8_t>(ctx->reader().read_uint32_field(0x67) & 0xff);
}

void VCEnabled::Write(hdb::HDBContext* ctx) const {
    if (!ctx) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, cache_e);
    ctx->writer().write_uint32_field(0x66, cache_f);
    ctx->writer().write_uint32_field(0x67, cache_g);
}

}  // namespace vc
}  // namespace xfiles
