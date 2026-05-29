// SPDX-License-Identifier: MIT
// VCStdAction.cpp - byte-direct Read (slot 12) decoder.

#include "vc/vcstd_action.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCStdAction::VCStdAction() noexcept : cache_e(0), cache_f(0), cache_h(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
}

VCStdAction::~VCStdAction() = default;

void VCStdAction::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    cache_e = ctx->reader().read_uint32_field(0x65);
    cache_f = ctx->reader().read_uint32_field(0x66);
    cache_h = ctx->reader().read_uint32_field(0x68);
}

void VCStdAction::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, cache_e);
    ctx->writer().write_uint32_field(0x66, cache_f);
    ctx->writer().write_uint32_field(0x68, cache_h);
}

}  // namespace vc
}  // namespace xfiles
