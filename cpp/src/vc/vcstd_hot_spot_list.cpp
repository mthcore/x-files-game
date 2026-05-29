// SPDX-License-Identifier: MIT
// VCStdHotSpotList.cpp - byte-direct Read (slot 12) decoder.

#include "vc/vcstd_hot_spot_list.h"
#include "hdb/hdb_context.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCStdHotSpotList::VCStdHotSpotList() noexcept  {
    std::memset(_base_scratch, 0, sizeof(_base_scratch));
}

VCStdHotSpotList::~VCStdHotSpotList() = default;

void VCStdHotSpotList::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;
    (void)ctx->reader().read_uint32_field(0x65);  // sub-coll
}

void VCStdHotSpotList::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    ctx->writer().write_uint32_field(0x65, 0);
}

}  // namespace vc
}  // namespace xfiles
