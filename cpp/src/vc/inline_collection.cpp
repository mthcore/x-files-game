// SPDX-License-Identifier: MIT
// InlineCollection implementation + items side-table.
//
// Adds adds count() / get_item_id(idx) per the on-disk format vtable[+0x18] / [+0x34].
// The items list lives in a side-table (file-local map) so that
// `sizeof(InlineCollection) == 60B` byte-direct stays intact.

#include "vc/inline_collection.h"
#include "hdb/hdb_context.h"
#include "hdb/hdb_tlv.h"

#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace xfiles {
namespace vc {

namespace {

// File-local side-table : maps InlineCollection address to its items list.
// Protected by a mutex since the engine may use multiple threads later (XMV
// playback queue runs concurrent with main game loop).
struct ItemsTable {
    std::mutex mu;
    std::unordered_map<const InlineCollection*, std::vector<uint32_t>> map;
};
ItemsTable& table() {
    static ItemsTable t;
    return t;
}

}  // namespace

InlineCollection::InlineCollection() noexcept
    : _vtable(nullptr),
      head_id(3),   // f08 = 3 means fresh/uninitialized (W21.31 byte-direct)
      f0c(0),
      f10(0),
      items_id(0),
      parent_ptr(nullptr),
      id_counter(0) {
    std::memset(_f04_07, 0, sizeof(_f04_07));
    std::memset(_f18_27, 0, sizeof(_f18_27));
    std::memset(_f30_3b, 0, sizeof(_f30_3b));
}

InlineCollection::~InlineCollection() noexcept {
    clear_items();
}

void InlineCollection::read_header(hdb::HDBContext* ctx, uint8_t tag) {
    if (!ctx) return;
    // Read a tagged TLV record that carries the items_id (CNeoIDList head).
    // The full the on-disk format behavior ( ->) wraps an array
    // iter via/d0, which a future revision will drive from master HDB.
    items_id = ctx->reader().read_uint32_field(tag);
}

void InlineCollection::write_header(hdb::HDBContext* ctx, uint8_t tag) const {
    if (!ctx) return;
    ctx->writer().write_uint32_field(tag, items_id);
}

std::size_t InlineCollection::count() const {
    auto& t = table();
    std::lock_guard<std::mutex> lock(t.mu);
    auto it = t.map.find(this);
    return (it == t.map.end()) ? 0u : it->second.size();
}

uint32_t InlineCollection::get_item_id(std::size_t idx) const {
    auto& t = table();
    std::lock_guard<std::mutex> lock(t.mu);
    auto it = t.map.find(this);
    if (it == t.map.end() || idx >= it->second.size()) return 0u;
    return it->second[idx];
}

void InlineCollection::set_item_ids(const std::vector<uint32_t>& ids) {
    auto& t = table();
    std::lock_guard<std::mutex> lock(t.mu);
    t.map[this] = ids;
}

void InlineCollection::clear_items() {
    auto& t = table();
    std::lock_guard<std::mutex> lock(t.mu);
    t.map.erase(this);
}

}  // namespace vc
}  // namespace xfiles
