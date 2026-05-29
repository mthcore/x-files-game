// SPDX-License-Identifier: MIT
// VCIdeaMap.cpp - byte-direct Read (slot 12) decoder.

#include "vc/vcidea_map.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCIdeaMap::VCIdeaMap() noexcept : cache_e(0), cache_f(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
}

VCIdeaMap::~VCIdeaMap() = default;

void VCIdeaMap::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    cache_e = ctx->reader().read_uint32_field(0x65);
    cache_f = ctx->reader().read_uint32_field(0x66);
}

void VCIdeaMap::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, cache_e);
    ctx->writer().write_uint32_field(0x66, cache_f);
}

}  // namespace vc
}  // namespace xfiles
