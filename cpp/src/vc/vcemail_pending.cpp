// SPDX-License-Identifier: MIT
// VCEmailPending — byte-exact Read/Write per W23.4 (base init + sub-coll tag 'f').
#include "vc/vcemail_pending.h"
#include "hdb/hdb_context.h"
#include <cstring>

namespace xfiles {
namespace vc {

VCEmailPending::VCEmailPending() noexcept : _npfl_flags(0), _vers(5) {
    std::memset(_engine_internal, 0, sizeof(_engine_internal));
    std::memset(_intermediate, 0, sizeof(_intermediate));
    std::memset(_tail, 0, sizeof(_tail));
    sub_coll.parent_ptr = this;
}

VCEmailPending::~VCEmailPending() = default;

void VCEmailPending::Read(HDBContext* ctx, uint32_t /*param_2*/) {
    if (!ctx) return;
    ctx->read_base_init(_npfl_flags, _vers);
    sub_coll.read_header(ctx, 0x66);
}

void VCEmailPending::Write(HDBContext* ctx) const {
    if (!ctx) return;
    ctx->write_base_init(_npfl_flags, _vers);
    sub_coll.write_header(ctx, 0x66);
}

}  // namespace vc
}  // namespace xfiles