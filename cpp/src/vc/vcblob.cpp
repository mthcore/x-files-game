// SPDX-License-Identifier: MIT
// VCBlob.cpp — byte-direct Read (slot 12) decoder.

#include "vc/vcblob.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCBlob::VCBlob() noexcept {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(sub_coll,      0, sizeof(sub_coll));
}

VCBlob::~VCBlob() = default;

/// Byte-direct Read decoder (vtable slot 12) :
/// base init
/// sub-coll Read 'e'
void VCBlob::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    (void)ctx->reader().read_uint32_field(0x65);  // sub-coll placeholder
}

void VCBlob::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, 0);
}

}  // namespace vc
}  // namespace xfiles
