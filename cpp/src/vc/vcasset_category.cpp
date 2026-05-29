// SPDX-License-Identifier: MIT
// VCAssetCategory.cpp — byte-direct Read (slot 12) decoder.

#include "vc/vcasset_category.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCAssetCategory::VCAssetCategory() noexcept : cache_e(0), cache_f(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(_pad_2d,       0, sizeof(_pad_2d));
}

VCAssetCategory::~VCAssetCategory() = default;

void VCAssetCategory::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    cache_e = ctx->reader().read_uint32_field(0x65);
    cache_f = static_cast<uint8_t>(
        ctx->reader().read_uint32_field(0x66) & 0xff);
}

void VCAssetCategory::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, cache_e);
    ctx->writer().write_uint32_field(0x66, static_cast<uint32_t>(cache_f));
}

}  // namespace vc
}  // namespace xfiles
