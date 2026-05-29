// SPDX-License-Identifier: MIT
// VCConversationHistory — byte-exact Read/Write per W23.4 (base init + sub-coll tag 'f').
#include "vc/vcconversation_history.h"
#include "hdb/hdb_context.h"
#include <cstring>

namespace xfiles {
namespace vc {

VCConversationHistory::VCConversationHistory() noexcept : _npfl_flags(0), _vers(5) {
    std::memset(_engine_internal, 0, sizeof(_engine_internal));
    std::memset(_intermediate, 0, sizeof(_intermediate));
    std::memset(_tail, 0, sizeof(_tail));
    sub_coll.parent_ptr = this;
}

VCConversationHistory::~VCConversationHistory() = default;

void VCConversationHistory::Read(HDBContext* ctx, uint32_t /*param_2*/) {
    if (!ctx) return;
    ctx->read_base_init(_npfl_flags, _vers);
    sub_coll.read_header(ctx, 0x66);
}

void VCConversationHistory::Write(HDBContext* ctx) const {
    if (!ctx) return;
    ctx->write_base_init(_npfl_flags, _vers);
    sub_coll.write_header(ctx, 0x66);
}

}  // namespace vc
}  // namespace xfiles