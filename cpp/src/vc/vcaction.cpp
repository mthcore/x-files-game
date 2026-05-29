// SPDX-License-Identifier: MIT
// VCAction.cpp — byte-direct Read (slot 12) decoder.

#include "vc/vcaction.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCAction::VCAction() noexcept
    : kind_byte(0), counter(0) {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
    std::memset(sub_coll,      0, sizeof(sub_coll));
    std::memset(_pad_65,       0, sizeof(_pad_65));
}

VCAction::~VCAction() = default;

/// Byte-direct Read decoder (vtable slot 12) :
/// base init
/// sub-coll Read 'e'
/// 599-byte switch
///                                                            // on kind_byte & 0x7f
void VCAction::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    // Sub-collection at this+0x28 read with tag 'e'. The reference decoder just consumes
    // the TLV record ; the actual map/list payload handling is deferred.
    (void)ctx->reader().read_uint32_field(0x65);
    // Byte field at this+0x64, tag 'f' (low byte of next record).
    kind_byte = static_cast<uint8_t>(
        ctx->reader().read_uint32_field(0x66) & 0xff);
    // post-Read dispatch (599-byte switch on kind_byte) is not
    // yet translated — it routes the action into the engine's runtime
    // execution per (kind_byte & 0x7f). Marker for future work.
}

void VCAction::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, 0);            // sub-coll placeholder
    ctx->writer().write_uint32_field(0x66, static_cast<uint32_t>(kind_byte));
}

}  // namespace vc
}  // namespace xfiles
