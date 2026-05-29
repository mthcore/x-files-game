// SPDX-License-Identifier: MIT
// NL (NeoLogic Persistent) ClassMeta — byte-direct Read decoder.
//
// the on-disk format maintains a global class registry indexed by
// (class_id + 0x2D). Each entry is a ClassMeta object holding :
//   +0x00 : vtable ptr (PTR_LAB_00664298 initially, then PTR_LAB_006642a8)
//   +0x04 : factory function VA
//   +0x08 : type_flag (0=user, 1=builtin)
//   +0x0C : class_id
//   +0x10 : parent ClassMeta ptr (if user class)
//   +0x14..+0x113 : name string (NUL-terminated, max 256 chars)
//   +0x114 : parent ClassMeta ptr (+0x45 in dword indexing)
//   +0x118 : reserved
//   +0x11C : type_size (= 7 for system / 0 for user)
//   +0x120 : (varies)
//   +0x128 : default_value (or fourcc 'ID  '=0x49442020 for system)
//
// In the reference implementation we hold ClassMeta as a typed struct + a flat
// `unordered_map<class_id, ClassMeta*>` registry (API-iso). Future iso work
// can swap to a byte-aligned global array if needed.

#ifndef NL_CLASS_META_H
#define NL_CLASS_META_H

#include <cstdint>
#include <string>
#include <unordered_map>

namespace xfiles {
namespace nl {

/// NL class kind. Matches the on-disk `type_flag` arg :
///   0 = user class (e.g. VC* classes 0x27..0x57)
///   1 = builtin/system class (CNeoPersist, CNeoIDList, etc.)
enum class ClassKind : uint8_t {
    User    = 0,
    System  = 1,
};

/// Factory function signature : returns a new instance of the registered
/// class. Args mirror the on-disk format's (heap_ptr, init_flag).
using FactoryFn = void* (*)(void* heap_ptr, uint32_t init_flag);

/// NL ClassMeta. Byte-direct fields from body.
struct ClassMeta {
    uint32_t   class_id;        ///< xfiles param_1 (e.g. 0x27 VCTitle)
    ClassKind  kind;            ///< xfiles param_2 (0=user, 1=system)
    std::string name;           ///< xfiles param_3 (e.g. "VCTitle")
    FactoryFn  factory;         ///< xfiles param_4 (factory)
    ClassMeta* parent;          ///< xfiles param_5 (resolved parent meta)
    uint32_t   default_value;   ///< xfiles param_6 (fourcc or zero)
};

/// Global class registry. Holds ClassMeta entries keyed by_class_id.
/// In the on-disk format this is a flat array indexed by (class_id+0x2D);
/// the reference implementation uses a hash map for simplicity (lookup semantic equivalent).
class ClassRegistry {
public:
    /// Singleton accessor — matches the global registry.
    static ClassRegistry& instance();

    /// Byte-direct Read decoder. Allocates a ClassMeta, fills its
    /// fields, and inserts it into the registry under `class_id`. Returns
    /// a non-owning pointer to the registered entry (lives for the program
    /// lifetime, like the on-disk format's static class metas).
    ///
    /// `parent` may be null for user classes : in the on-disk format the ctor walks the
    /// class hierarchy via the factory's parent-chain ; here callers pass
    /// the resolved parent meta explicitly.
    ClassMeta* register_class(uint32_t   class_id,
                              ClassKind  kind,
                              const std::string& name,
                              FactoryFn  factory,
                              ClassMeta* parent,
                              uint32_t   default_value);

    /// Lookup a registered class by_class_id. Returns nullptr if not found.
    ClassMeta* find(uint32_t class_id) const;

    /// Lookup by name (linear scan — used for diagnostics + the D1 trace
    /// reconstruction). Returns nullptr if no class with that name exists.
    ClassMeta* find_by_name(const std::string& name) const;

    /// Number of registered classes.
    std::size_t size() const { return entries_.size(); }

    /// Reset the registry (test helper).
    void clear();

private:
    ClassRegistry() = default;
    ~ClassRegistry();
    ClassRegistry(const ClassRegistry&) = delete;
    ClassRegistry& operator=(const ClassRegistry&) = delete;

    std::unordered_map<uint32_t, ClassMeta*> entries_;
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASS_META_H
