// SPDX-License-Identifier: MIT
// VCPlayer.cpp - byte-direct decoder (inherits VCCharacter Read).

#include "vc/vcplayer.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCPlayer::VCPlayer() noexcept : cache_e(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
}

VCPlayer::~VCPlayer() = default;

void VCPlayer::Read(hdb::HDBContext* ctx, uint32_t) {
    if (!ctx) return;
    uint32_t npfl=0, vers=0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    cache_e = ctx->reader().read_uint32_field(0x65);
}

void VCPlayer::Write(hdb::HDBContext* ctx) const {
    if (!ctx) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, cache_e);
}

}  // namespace vc
}  // namespace xfiles
