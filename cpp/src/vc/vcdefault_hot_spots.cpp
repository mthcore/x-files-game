// SPDX-License-Identifier: MIT
// VCDefaultHotSpots.cpp - byte-direct Read (slot 12) decoder.
// Identical pattern to VCDefaultCursors.

#include "vc/vcdefault_hot_spots.h"
#include "hdb/hdb_context.h"

#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace xfiles {
namespace vc {

namespace {
struct DefHotSpotsItems {
    std::mutex mu;
    std::unordered_map<const VCDefaultHotSpots*, std::vector<uint32_t>> map;
};
DefHotSpotsItems& table() { static DefHotSpotsItems t; return t; }
}  // namespace

VCDefaultHotSpots::VCDefaultHotSpots() noexcept {
    std::memset(_base_scratch,    0, sizeof(_base_scratch));
    std::memset(sub_coll_compact, 0, sizeof(sub_coll_compact));
}

VCDefaultHotSpots::~VCDefaultHotSpots() {
    auto& t = table();
    std::lock_guard<std::mutex> lk(t.mu);
    t.map.erase(this);
}

void VCDefaultHotSpots::set_item_count(std::size_t n) {
    auto& t = table();
    std::lock_guard<std::mutex> lk(t.mu);
    t.map[this].assign(n, 0u);
}

std::size_t VCDefaultHotSpots::item_count() const {
    auto& t = table();
    std::lock_guard<std::mutex> lk(t.mu);
    auto it = t.map.find(this);
    return (it == t.map.end()) ? 0u : it->second.size();
}

uint32_t VCDefaultHotSpots::get_item_id(std::size_t idx) const {
    auto& t = table();
    std::lock_guard<std::mutex> lk(t.mu);
    auto it = t.map.find(this);
    if (it == t.map.end() || idx >= it->second.size()) return 0u;
    return it->second[idx];
}

void VCDefaultHotSpots::Read(hdb::HDBContext* ctx, uint32_t /*mode*/) {
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

void VCDefaultHotSpots::Write(hdb::HDBContext* ctx) const {
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
