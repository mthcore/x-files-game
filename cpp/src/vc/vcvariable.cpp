// SPDX-License-Identifier: MIT
// VCVariable.cpp - byte-direct Read decoder.

#include "vc/vcvariable.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCVariable::VCVariable() noexcept
    : cache_g(0), cache_i(0), cache_h(0), cache_f(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(sub_object,    0, sizeof(sub_object));
    std::memset(_pad_42,       0, sizeof(_pad_42));
}

VCVariable::~VCVariable() = default;

void VCVariable::Read(hdb::HDBContext* ctx, uint32_t) {
    if (!ctx) return;
    uint32_t npfl=0, vers=0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    (void)ctx->reader().read_uint32_field(0x65);
    cache_g = ctx->reader().read_uint32_field(0x67);
    cache_i = ctx->reader().read_uint32_field(0x69);
    cache_h = static_cast<uint8_t>(ctx->reader().read_uint32_field(0x68) & 0xff);
    cache_f = static_cast<uint8_t>(ctx->reader().read_uint32_field(0x66) & 0xff);
}

void VCVariable::Write(hdb::HDBContext* ctx) const {
    if (!ctx) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, 0);
    ctx->writer().write_uint32_field(0x67, cache_g);
    ctx->writer().write_uint32_field(0x69, cache_i);
    ctx->writer().write_uint32_field(0x68, static_cast<uint32_t>(cache_h));
    ctx->writer().write_uint32_field(0x66, static_cast<uint32_t>(cache_f));
}

}  // namespace vc
}  // namespace xfiles
