// SPDX-License-Identifier: MIT
// VCExplorable.cpp — byte-direct Read (slot 12) decoder.

#include "vc/vcexplorable.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCExplorable::VCExplorable() noexcept
    : sub_object_ptr(0), heap_owned(0), cache_f(0), field_48(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(sub_fields,    0, sizeof(sub_fields));
    std::memset(more_fields,   0, sizeof(more_fields));
}

VCExplorable::~VCExplorable() = default;

/// Byte-direct Read decoder (vtable slot 12) :
/// base init
/// >vtable[+0x08])(this+0x28, ctx, 0x65, 0); // sub Read 'e'
void VCExplorable::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    // Sub-object Read at this+0x28, tag 'e' — consume TLV record.
    (void)ctx->reader().read_uint32_field(0x65);
    cache_f = ctx->reader().read_uint32_field(0x66);
}

void VCExplorable::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, 0);
    ctx->writer().write_uint32_field(0x66, cache_f);
}

}  // namespace vc
}  // namespace xfiles
