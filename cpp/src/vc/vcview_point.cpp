// SPDX-License-Identifier: MIT
// VCViewPoint.cpp — byte-direct Read (slot 12) decoder.

#include "vc/vcview_point.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCViewPoint::VCViewPoint() noexcept
    : cache_g(0), cache_h(0), field_78(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(sub_coll,      0, sizeof(sub_coll));
    std::memset(sub_object,    0, sizeof(sub_object));
}

VCViewPoint::~VCViewPoint() = default;

void VCViewPoint::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    (void)ctx->reader().read_uint32_field(0x65);  // sub-coll
    (void)ctx->reader().read_uint32_field(0x66);  // sub-object
    cache_g = ctx->reader().read_uint32_field(0x67);
    cache_h = ctx->reader().read_uint32_field(0x68);
}

void VCViewPoint::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, 0);
    ctx->writer().write_uint32_field(0x66, 0);
    ctx->writer().write_uint32_field(0x67, cache_g);
    ctx->writer().write_uint32_field(0x68, cache_h);
}

}  // namespace vc
}  // namespace xfiles
