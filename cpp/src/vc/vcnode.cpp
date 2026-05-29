// SPDX-License-Identifier: MIT
// VCNode.cpp — byte-direct Read (slot 12) decoder.

#include "vc/vcnode.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCNode::VCNode() noexcept
    : field_60(0), cache_f(0), cache_g(0), field_6c(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(sub_object,    0, sizeof(sub_object));
    std::memset(payload,       0, sizeof(payload));
}

VCNode::~VCNode() = default;

/// Byte-direct Read decoder (vtable slot 12) :
/// base init
/// sub-obj read 'e'
void VCNode::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    // reads the sub-object at this+0x28 with tag 'e'.
    // The reference decoder consumes the TLV record ; full sub-object payload deferred.
    (void)ctx->reader().read_uint32_field(0x65);
    cache_f = ctx->reader().read_uint32_field(0x66);
    cache_g = ctx->reader().read_uint32_field(0x67);
}

void VCNode::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, 0);   // sub-obj placeholder
    ctx->writer().write_uint32_field(0x66, cache_f);
    ctx->writer().write_uint32_field(0x67, cache_g);
}

}  // namespace vc
}  // namespace xfiles
