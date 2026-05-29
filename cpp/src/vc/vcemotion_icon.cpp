// SPDX-License-Identifier: MIT
// VCEmotionIcon.cpp - byte-direct Read (slot 12) decoder; thunks to the shared
// Icon-family reader. Body identical for all 5 Icon family classes.

#include "vc/vcemotion_icon.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCEmotionIcon::VCEmotionIcon() noexcept
    : cache_e(0), cache_f(0), cache_g(0), cache_h(0),
      cache_k(0), cache_i(0), cache_j(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
}

VCEmotionIcon::~VCEmotionIcon() = default;

void VCEmotionIcon::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl;
    cache_e = ctx->reader().read_uint32_field(0x65);
    cache_f = ctx->reader().read_uint32_field(0x66);
    cache_g = ctx->reader().read_uint32_field(0x67);
    cache_h = ctx->reader().read_uint32_field(0x68);
    cache_i = ctx->reader().read_uint32_field(0x69);
    cache_j = ctx->reader().read_uint32_field(0x6a);
    // 'k' (0x6b) only present if version > 3 � gated path in the on-disk format.
    if (vers > 3) {
        cache_k = ctx->reader().read_uint32_field(0x6b);
    }
}

void VCEmotionIcon::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 4);  // emit vers=4 so 'k' field is always written
    ctx->writer().write_uint32_field(0x65, cache_e);
    ctx->writer().write_uint32_field(0x66, cache_f);
    ctx->writer().write_uint32_field(0x67, cache_g);
    ctx->writer().write_uint32_field(0x68, cache_h);
    ctx->writer().write_uint32_field(0x69, cache_i);
    ctx->writer().write_uint32_field(0x6a, cache_j);
    ctx->writer().write_uint32_field(0x6b, cache_k);
}

}  // namespace vc
}  // namespace xfiles
