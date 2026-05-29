// SPDX-License-Identifier: MIT
// VCAssetRef.cpp - byte-direct Read decoder.

#include "vc/vcasset_ref.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCAssetRef::VCAssetRef() noexcept
    : cache_f(0), cache_g(0), cache_h(0), cache_i(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(sub_object, 0, sizeof(sub_object));
    std::memset(_pad_42, 0, sizeof(_pad_42));
}

VCAssetRef::~VCAssetRef() = default;

void VCAssetRef::Read(hdb::HDBContext* ctx, uint32_t) {
    if (!ctx) return;
    uint32_t npfl=0, vers=0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    (void)ctx->reader().read_uint32_field(0x65);
    cache_f = ctx->reader().read_uint32_field(0x66);
    cache_g = ctx->reader().read_uint32_field(0x67);
    cache_h = static_cast<uint8_t>(ctx->reader().read_uint32_field(0x68) & 0xff);
    cache_i = static_cast<uint8_t>(ctx->reader().read_uint32_field(0x69) & 0xff);
}

void VCAssetRef::Write(hdb::HDBContext* ctx) const {
    if (!ctx) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, 0);
    ctx->writer().write_uint32_field(0x66, cache_f);
    ctx->writer().write_uint32_field(0x67, cache_g);
    ctx->writer().write_uint32_field(0x68, static_cast<uint32_t>(cache_h));
    ctx->writer().write_uint32_field(0x69, static_cast<uint32_t>(cache_i));
}

}  // namespace vc
}  // namespace xfiles
