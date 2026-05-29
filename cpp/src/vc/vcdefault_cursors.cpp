// SPDX-License-Identifier: MIT
// VCDefaultCursors.cpp - byte-direct Read (slot 12) decoder.
//
// the on-disk format body :
//(ctx, mode)                                 // base init
//   sub_coll = this+0x28
//   if sub_coll->vtable[+0x18](sub_coll) != 0:              // has items?
//     get_item = sub_coll->vtable[+0x34]
//     tag = 0x65
//     for idx = 0 ; idx < count ; idx++, tag++ :
//       item_ptr = get_item(sub_coll, idx)
//       *item_ptr = ctx->vtable[+0x9c](tag)                  // read uint32
//
// In the reference implementation, the compact 32B sub-coll cannot embed item ids directly so
// the items live in a side-table keyed by `this`. a future revision will populate
// counts from the master HDB walker ; tests can set them manually.

#include "vc/vcdefault_cursors.h"
#include "hdb/hdb_context.h"

#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace xfiles {
namespace vc {

namespace {
struct DefCursorsItems {
    std::mutex mu;
    std::unordered_map<const VCDefaultCursors*, std::vector<uint32_t>> map;
};
DefCursorsItems& table() { static DefCursorsItems t; return t; }
}  // namespace

VCDefaultCursors::VCDefaultCursors() noexcept {
    std::memset(_base_scratch,    0, sizeof(_base_scratch));
    std::memset(sub_coll_compact, 0, sizeof(sub_coll_compact));
}

VCDefaultCursors::~VCDefaultCursors() {
    auto& t = table();
    std::lock_guard<std::mutex> lk(t.mu);
    t.map.erase(this);
}

void VCDefaultCursors::set_item_count(std::size_t n) {
    auto& t = table();
    std::lock_guard<std::mutex> lk(t.mu);
    t.map[this].assign(n, 0u);
}

std::size_t VCDefaultCursors::item_count() const {
    auto& t = table();
    std::lock_guard<std::mutex> lk(t.mu);
    auto it = t.map.find(this);
    return (it == t.map.end()) ? 0u : it->second.size();
}

uint32_t VCDefaultCursors::get_item_id(std::size_t idx) const {
    auto& t = table();
    std::lock_guard<std::mutex> lk(t.mu);
    auto it = t.map.find(this);
    if (it == t.map.end() || idx >= it->second.size()) return 0u;
    return it->second[idx];
}

void VCDefaultCursors::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
    if (ctx == nullptr) return;
    uint32_t npfl = 0, vers = 0;
    ctx->read_base_init(npfl, vers);
    (void)npfl; (void)vers;

    std::size_t n = item_count();
    if (n == 0) return;

    auto& t = table();
    std::lock_guard<std::mutex> lk(t.mu);
    auto& v = t.map[this];
    uint8_t tag = 0x65;
    for (std::size_t i = 0; i < n; ++i, ++tag) {
        v[i] = ctx->reader().read_uint32_field(tag);
    }
}

void VCDefaultCursors::Write(hdb::HDBContext* ctx) const {
    if (ctx == nullptr) return;
    ctx->write_base_init(0, 0);
    std::size_t n = item_count();
    uint8_t tag = 0x65;
    for (std::size_t i = 0; i < n; ++i, ++tag) {
        ctx->writer().write_uint32_field(tag, get_item_id(i));
    }
}

}  // namespace vc
}  // namespace xfiles
