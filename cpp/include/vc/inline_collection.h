// SPDX-License-Identifier: MIT
// InlineCollection — 60-byte embedded sub-collection.
//
// Byte-direct size : 60 = 0x3c
// A standard InlineCollection plus several custom variants :
//   - VCEmail family + VCPDANotes (11/13 slots shared)
//   - VCConversationHistory
//   - VCPDANotes alternate slot
//
// Layout (byte-direct from W21.31 + W22.1 dumps) :
//   +0x00 vtable
//   +0x08 head_id  (CNeoIDList persistent ID)
//   +0x0c f0c
//   +0x10 f10
//   +0x14 items_id (CNeoIDList head)
//   +0x18..+0x27 internal state
//   +0x28 parent_ptr (back-reference to containing object)
//   +0x2c id_counter
//   +0x30..+0x3b pad/reserved

#ifndef VC_INLINE_COLLECTION_H
#define VC_INLINE_COLLECTION_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include "vc/_size_assert.h"

namespace xfiles {
namespace hdb { class HDBContext; }  // forward decl
namespace vc {

class VCObject;  // forward decl

struct InlineCollection {
    void* _vtable;            // +0x00
    char _f04_07[4];          // +0x04
    uint32_t head_id;         // +0x08
    uint32_t f0c;             // +0x0c
    uint32_t f10;             // +0x10
    uint32_t items_id;        // +0x14
    char _f18_27[16];         // +0x18..+0x27
    VCObject* parent_ptr;     // +0x28 — back-reference to container
    uint32_t id_counter;      // +0x2c
    char _f30_3b[12];         // +0x30..+0x3b

    InlineCollection() noexcept;
    ~InlineCollection() noexcept;

    /// Read sub-collection header from a tagged TLV record.
    /// Equivalent to the on-disk sub-object read (this, ctx, tag, parent).
    void read_header(hdb::HDBContext* ctx, uint8_t tag);
    void write_header(hdb::HDBContext* ctx, uint8_t tag) const;

    /// Items count — equivalent to the on-disk `(*this->vtable[+0x18])(this)`.
    /// In the on-disk format the count is computed by walking the CNeoIDList starting at
    /// `items_id`. In the reference implementation we maintain a side-table (keyed by `this`)
    /// populated either by the master HDB walker (when items_id resolves) or
    /// by direct injection via `set_item_ids()` (test fixtures, Reg*Vars).
    std::size_t count() const;

    /// Get item persistent ID at `idx` — equivalent to the on-disk format
    /// `(*this->vtable[+0x34])(this, idx)`. Returns 0 if out of range. Resolve
    /// to a `VCObject*` via `hdb::MasterHDB::lookup(id)`.
    uint32_t get_item_id(std::size_t idx) const;

    /// Populate items list. In iso runtime this would be triggered by the
    /// master HDB walker following the linked CNeoIDList from `items_id`.
    /// Exposed publicly so tests + a future revision walker can drive it.
    void set_item_ids(const std::vector<uint32_t>& ids);

    /// Clear the items side-table (call from dtor + clear ops).
    void clear_items();
};

XFILES_ASSERT_SIZE(InlineCollection, 0x3c, "InlineCollection must be 60 bytes");
XFILES_ASSERT_OFFSET(InlineCollection, head_id, 0x08, "head_id offset");
XFILES_ASSERT_OFFSET(InlineCollection, items_id, 0x14, "items_id offset");
XFILES_ASSERT_OFFSET(InlineCollection, parent_ptr, 0x28, "parent_ptr offset");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_INLINE_COLLECTION_H
