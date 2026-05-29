// SPDX-License-Identifier: MIT
// VCName.cpp - byte-direct Read decoder.

#include "vc/vcname.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCName::VCName() noexcept {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(sub_object_e, 0, sizeof(sub_object_e));
    std::memset(sub_object_f, 0, sizeof(sub_object_f));
}

VCName::~VCName() = default;

void VCName::Read(hdb::HDBContext* ctx, uint32_t) {
    if (!ctx) return;
    uint32_t npfl=0, vers=0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    (void)ctx->reader().read_uint32_field(0x65);
    (void)ctx->reader().read_uint32_field(0x66);
}

void VCName::Write(hdb::HDBContext* ctx) const {
    if (!ctx) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, 0);
    ctx->writer().write_uint32_field(0x66, 0);
}

}  // namespace vc
}  // namespace xfiles
