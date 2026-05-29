// SPDX-License-Identifier: MIT
// VCTitle.cpp - byte-direct Read decoder.

#include "vc/vctitle.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCTitle::VCTitle() noexcept
    : cache_j(0), cache_k(0), cache_l(0), cache_m(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(sub_coll,      0, sizeof(sub_coll));
    std::memset(sub_object_f,  0, sizeof(sub_object_f));
    std::memset(sub_object_g,  0, sizeof(sub_object_g));
    std::memset(sub_object_h,  0, sizeof(sub_object_h));
    std::memset(sub_object_i,  0, sizeof(sub_object_i));
}

VCTitle::~VCTitle() = default;

void VCTitle::Read(hdb::HDBContext* ctx, uint32_t) {
    if (!ctx) return;
    uint32_t npfl=0, vers=0;
    ctx->read_base_init(npfl, vers);
    (void)npfl;
    if (vers > 1) {
        (void)ctx->reader().read_uint32_field(0x65);  // sub-coll
    }
    // 4 sub-objects read in tag order f / h / g / i (on-disk tag-order quirk).
    (void)ctx->reader().read_uint32_field(0x66);  // sub_object_f
    (void)ctx->reader().read_uint32_field(0x68);  // sub_object_h
    (void)ctx->reader().read_uint32_field(0x67);  // sub_object_g
    (void)ctx->reader().read_uint32_field(0x69);  // sub_object_i
    cache_j = ctx->reader().read_uint32_field(0x6a);
    cache_k = ctx->reader().read_uint32_field(0x6b);
    cache_l = static_cast<uint16_t>(ctx->reader().read_uint32_field(0x6c) & 0xffff);
    cache_m = static_cast<uint16_t>(ctx->reader().read_uint32_field(0x6d) & 0xffff);
}

void VCTitle::Write(hdb::HDBContext* ctx) const {
    if (!ctx) return;
    ctx->write_base_init(0, 2);  // vers=2 so sub-coll is written
    ctx->writer().write_uint32_field(0x65, 0);
    ctx->writer().write_uint32_field(0x66, 0);
    ctx->writer().write_uint32_field(0x68, 0);
    ctx->writer().write_uint32_field(0x67, 0);
    ctx->writer().write_uint32_field(0x69, 0);
    ctx->writer().write_uint32_field(0x6a, cache_j);
    ctx->writer().write_uint32_field(0x6b, cache_k);
    ctx->writer().write_uint32_field(0x6c, static_cast<uint32_t>(cache_l));
    ctx->writer().write_uint32_field(0x6d, static_cast<uint32_t>(cache_m));
}

}  // namespace vc
}  // namespace xfiles
