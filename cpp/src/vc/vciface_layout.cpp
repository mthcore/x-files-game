// SPDX-License-Identifier: MIT
// VCIfaceLayout.cpp - byte-direct Read decoder.

#include "vc/vciface_layout.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCIfaceLayout::VCIfaceLayout() noexcept : cache_f(0), cache_g(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(sub_coll,      0, sizeof(sub_coll));
    std::memset(_pad_69,       0, sizeof(_pad_69));
}

VCIfaceLayout::~VCIfaceLayout() = default;

void VCIfaceLayout::Read(hdb::HDBContext* ctx, uint32_t) {
    if (!ctx) return;
    uint32_t npfl=0, vers=0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    (void)ctx->reader().read_uint32_field(0x65);  // sub-coll
    cache_f = ctx->reader().read_uint32_field(0x66);
    cache_g = static_cast<uint8_t>(ctx->reader().read_uint32_field(0x67) & 0xff);
}

void VCIfaceLayout::Write(hdb::HDBContext* ctx) const {
    if (!ctx) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, 0);
    ctx->writer().write_uint32_field(0x66, cache_f);
    ctx->writer().write_uint32_field(0x67, static_cast<uint32_t>(cache_g));
}

}  // namespace vc
}  // namespace xfiles
