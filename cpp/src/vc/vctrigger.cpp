// SPDX-License-Identifier: MIT
// VCTrigger.cpp — byte-direct Read decoder (Read, vtable slot 12).

#include "vc/vctrigger.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCTrigger::VCTrigger() noexcept : cache_e(0), cache_f(0) {
    std::memset(_base_scratch,      0, sizeof(_base_scratch));
    std::memset(subcoll_conditions, 0, sizeof(subcoll_conditions));
    std::memset(subcoll_actions,    0, sizeof(subcoll_actions));
    std::memset(subcoll_targets,    0, sizeof(subcoll_targets));
    std::memset(_pad_e9,            0, sizeof(_pad_e9));
}

VCTrigger::~VCTrigger() = default;

uint32_t VCTrigger::eval(uint32_t /*arg1*/, uint32_t /*arg2*/) {
    // (universal Eval) handles trigger eval ; this slot is a
    // thunk that delegates. Until universal eval is wired we return 0.
    return 0;
}

/// Byte-direct Read decoder (vtable slot 12) :
/// base init NPfl/vers
///     iVar1 = *param_1;                              // ctx vtable
void VCTrigger::Read(hdb::HDBContext* ctx, uint32_t mode) {
    if (ctx == nullptr) return;
    // Base init (NPfl flag word + optional 'vers' dword).
    // additionally clears/sets bits at this+0x10/+0x14 ; we treat
    // _base_scratch as opaque since the reference implementation serializer doesn't use
    // those bits.
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers; (void)mode;
    // Two scalar fields. The 3 inline sub-collections (Conditions/Actions/
    // Targets) are NOT read here — they are populated via the Attach pattern
    // on the parent context's vtable when sub-objects register themselves.
    cache_e = ctx->reader().read_uint32_field(0x65);
    // returns undefined1 (byte) from ctx vtable[+0x80] for tag
    // 'f'. In the GAM/HDB record stream each entry is 8 bytes wide ; we take
    // the low byte of the 32-bit value as the byte read.
    cache_f = static_cast<uint8_t>(ctx->reader().read_uint32_field(0x66) & 0xff);
}

void VCTrigger::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    // Symmetric to Read : base init + 2 scalar TLV writes.
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, cache_e);
    ctx->writer().write_uint32_field(0x66, static_cast<uint32_t>(cache_f));
}

}  // namespace vc
}  // namespace xfiles
