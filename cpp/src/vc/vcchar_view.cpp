// SPDX-License-Identifier: MIT
// VCCharView.cpp — byte-direct Read (slot 12) decoder.

#include "vc/vcchar_view.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCCharView::VCCharView() noexcept
    : cache_e(0), cache_g(0), cache_h(0), cache_i(0), cache_j(0), field_58(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(sub_object,    0, sizeof(sub_object));
}

VCCharView::~VCCharView() = default;

void VCCharView::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    cache_e = ctx->reader().read_uint32_field(0x65);
    (void)ctx->reader().read_uint32_field(0x66);   // sub_object
    cache_g = ctx->reader().read_uint32_field(0x67);
    cache_h = ctx->reader().read_uint32_field(0x68);
    cache_i = ctx->reader().read_uint32_field(0x69);
    cache_j = ctx->reader().read_uint32_field(0x6a);
}

void VCCharView::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, cache_e);
    ctx->writer().write_uint32_field(0x66, 0);
    ctx->writer().write_uint32_field(0x67, cache_g);
    ctx->writer().write_uint32_field(0x68, cache_h);
    ctx->writer().write_uint32_field(0x69, cache_i);
    ctx->writer().write_uint32_field(0x6a, cache_j);
}

}  // namespace vc
}  // namespace xfiles
