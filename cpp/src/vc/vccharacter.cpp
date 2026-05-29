// SPDX-License-Identifier: MIT
// VCCharacter.cpp — byte-direct Read (slot 12) decoder.

#include "vc/vccharacter.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCCharacter::VCCharacter() noexcept : cache_e(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
}

VCCharacter::~VCCharacter() = default;

void VCCharacter::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    cache_e = ctx->reader().read_uint32_field(0x65);
}

void VCCharacter::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, cache_e);
}

}  // namespace vc
}  // namespace xfiles
