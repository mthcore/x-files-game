// SPDX-License-Identifier: MIT
// Master HDB — runtime ID lookup + B-tree walker.
//
// on-disk model (factory + chain) :
//   The master HDB holds an "items table" whose `[+0x5c]` field is an index
//   structure (B-tree internal pages 0xC2 + leaf pages 0xC3). To resolve an
//   ID to a record, the engine calls `items_table[+0x5c]->vtable[+0x78](id)`
//   which descends the B-tree.
//
// Reference decoder approach :
//   Without the internal B-tree page format documented (key/child-ptr layout
//   not visible on disk), this implements the **same end result** via
//   a flat index built at load time : scan all leaf records once, hash by
//   their externally-visible ID (the `value32` field of an 8-byte record).
//   `MasterHDB::resolve_record(id)` returns the record bytes or nullptr.
//
//   The legacy `register_object(id, VCObject*)` + `lookup(id)` API stays
//   for in-memory live objects (already used by 31 reference-implementation handlers).

#ifndef MASTER_HDB_H
#define MASTER_HDB_H

#include "hdb/hdb_container.h"

#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <vector>

namespace xfiles {
namespace vc { class VCObject; }
namespace hdb {

/// Lookup result for a master-HDB record.
struct RecordLocation {
    std::size_t page_idx;   ///< Index in HDBContainer pages
    std::size_t rec_idx;    ///< 8-byte record index within the page
    Record8     record;     ///< Parsed record fields
};

class MasterHDB {
public:
    MasterHDB();
    ~MasterHDB();

    /// Wire to an HDB file (XFILES.HDB) and build the flat ID index.
    /// Returns false if the container is null or empty.
    /// Equivalent to the on-disk format's master HDB ctor + initial B-tree page load.
    bool attach(HDBContainer* container);

    /// Resolve a persistent ID to the record bytes in the HDB. Returns
    /// `false` if not found. The output `loc` describes where the record
    /// lives so callers can fetch contextual bytes (string blobs, etc.)
    /// near it.
    bool resolve_record(uint32_t id, RecordLocation* loc) const;

    /// Step: resolve a persistent ID to the full TLV-encoded
    /// payload bytes for the corresponding VC object record. Follows the
    /// indirection : (page_idx, rec_idx) → Record8.flag16 → target page.
    /// Returns nullptr/0 on miss or if the indirection target is outside
    /// the HDB range.
    const uint8_t* resolve_payload(uint32_t id, std::size_t* out_size) const;

    /// Step: resolve an asset_id (game-resource ID)
    /// to its raw byte offset inside the HDB file. Walks the flat asset-index
    /// pages whose body holds 8-byte `(file_offset BE u32, asset_id BE u32)`
    /// pairs sorted by asset_id (e.g. page 12811 carries the menu video IDs
    /// 0xc1af..0xc1d7). Returns 0 if asset_id is not indexed.
    ///
    /// Derived from XFILES.HDB structure
    /// (tools/hdb_leaf_recon).  This is the on-disk equivalent of the on-disk format's
    /// `master_hdb->items_table->vtable[+0x78](id)` B-tree descent.
    uint32_t resolve_asset_file_offset(uint32_t asset_id) const;

    /// Cycle 23 (2026-05-15) /goal iso fonctionnel : resolve a script
    /// `action_id` (used by HOT files + the dispatcher) to its
    /// destination file offset in HDB. Walks all "tag 0x00" pages whose
    /// body holds 8-byte `(action_id BE u32, file_offset BE u32)` pairs.
    ///
    /// Example : action_id 2467 (0x09a3) -> file_offset  at
    /// HDB byte  (page tag 0x00).
    ///
    /// Returns 0 if not indexed. The file offset itself points to a
    /// typed record (usually a `c2 30 ...` marker followed by per-action
    /// metadata).
    uint32_t resolve_action_id(uint32_t action_id) const;

    /// Decoded action record fields (cycle 24 /goal iso fonctionnel).
    /// The record at file_offset starts with a `c2 30 00 00 00 01 ?? ??`
    /// marker followed by a typed payload + an ASCII tail (often a scene
    /// production name like "011 - Hangar Open ext").
    struct ActionRecord {
        uint32_t    file_off;       ///< file_offset where the record lives
        uint16_t    marker_flag16;  ///< bytes [6..7] of the marker (BE u16)
        uint16_t    payload_high;   ///< first u16 of the 8-byte payload
        uint16_t    payload_class;  ///< second u16 (often 0x0112 etc.)
        std::string ascii_tail;     ///< trailing ASCII text (scene name etc.)
        std::string ascii_chain;    ///< cycle 25 : concatenated ASCII across
                                    ///< up to 8 consecutive c2 30 records.
        std::string asset_fragments;///< cycle 30 : ONLY ASCII from records
                                    ///< with payload_class 0x0113 (asset
                                    ///< filename pieces). 0x0112 records
                                    ///< carry "011 - " scene-prefix noise.
    };

    /// Resolve an action_id and decode its target record into a structured
    /// view. Returns true if the action_id is indexed AND the record marker
    /// invariant holds.
    bool read_action_id_record(uint32_t action_id, ActionRecord* out) const;

    /// Step: count walked leaf records grouped by
    /// `(marker_tag, marker_sub_tag)`. Drives byte-direct cataloguing of HDB
    /// content by_class hint. Each leaf record was extracted via
    /// `nl::walk_leaf_records`.
    ///
    /// Returns a flat vector of triples `(marker_tag, marker_sub_tag, count)`
    /// sorted lexicographically. Useful for verifying that the walker hits
    /// the expected classes (e.g. how many `c3 31` records correspond to
    /// VCConversation instances).
    struct LeafMarkerCount {
        uint8_t marker_tag;
        uint8_t marker_sub_tag;
        uint32_t count;
    };
    std::vector<LeafMarkerCount> leaf_marker_histogram() const;

    /// Strict byte-direct B-tree descent from root.
    ///
    /// Iso path : start at the B-tree root page (the first 0xC2 internal page
    /// in HDB), find the child whose key range contains `id`, descend until
    /// a 0xC3 leaf page is reached, then locate the record whose `value32`
    /// matches `id`. Mirrors `items_table->vtable[+0x78](id)` from the on-disk format
    /// 
    ///
    /// Fills `loc` and returns true on success. Returns false if the descent
    /// hits a dead-end (no matching child key range) OR if no leaf record
    /// in the resolved leaf page matches the ID.
    ///
    /// NOTE : the flat hash built by attach() is the FAST PATH and remains
    /// the default. This B-tree walker is the iso-strict alternative.
    bool resolve_via_btree(uint32_t id, RecordLocation* loc) const;

    /// Strict : single B-tree descent step.
    /// Given a starting page index (root or internal), find the child page
    /// for a given ID, returning the next page index OR (size_t)-1 on miss.
    std::size_t btree_descend_one(std::size_t page_idx, uint32_t id) const;

    /// Strict : index of the root 0xC2 internal page.
    /// In XFILES.HDB, the B-tree root is typically the FIRST 0xC2 page (it
    /// has no parent reference and is the entry point for descent).
    std::size_t btree_root_page_index() const noexcept { return btree_root_; }

    /// Convenience : count of indexed records (flat hash size).
    std::size_t record_count() const { return id_index_.size(); }

    /// HDB container access (for sub-coll walks that need adjacent pages).
    HDBContainer* container() { return container_; }
    const HDBContainer* container() const { return container_; }

    // --- Live-object API (kept from earlier phases) ---

    /// Register an in-memory live object with a persistent ID.
    void register_object(uint32_t id, vc::VCObject* obj);

    /// Look up live object by ID. Returns nullptr if not registered.
    vc::VCObject* lookup(uint32_t id) const;

    /// Clear all in-memory registrations (not the HDB index).
    void clear_objects();

    std::size_t size() const { return id_map_.size(); }

private:
    HDBContainer* container_;
    // Flat index : ID (value32) -> (page_idx, rec_idx). Built once by attach().
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> id_index_;
    // Live in-memory objects (orthogonal to id_index_ which points to bytes).
    std::unordered_map<uint32_t, vc::VCObject*> id_map_;
    // Strict : B-tree root page index, discovered at attach() time.
    std::size_t btree_root_ = SIZE_MAX;
};

/// Lazy ID resolver — byte-direct iso (210B).
///
/// Holds a (master_hdb, ID) pair and resolves lazily on first `get()`.
/// Mirrors the on-disk format's lazy struct used by the lazy resolver + script-arg extraction.
class HDBLazyResolver {
public:
    HDBLazyResolver(MasterHDB* hdb, uint32_t id) noexcept
        : hdb_(hdb), id_(id), cached_(false), record_{0,0,0,0} {}

    /// Resolve (cached). Returns true if record was found, false otherwise.
    bool get(RecordLocation* out_loc);

    uint32_t id() const { return id_; }
    bool     is_cached() const { return cached_; }

private:
    MasterHDB* hdb_;
    uint32_t   id_;
    bool       cached_;
    RecordLocation cached_loc_{};
    Record8        record_;
};

}  // namespace hdb
}  // namespace xfiles

#endif  // MASTER_HDB_H
