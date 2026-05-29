// SPDX-License-Identifier: MIT
// NL system class bootstrap — registers the 5+ CNeo* classes that the on-disk format
// instantiates during CRT init (BEFORE WinMain). Reference :
//
//   the system-class set (28 classes)
//
// Captured hits 1..25 :
//   #1  CNeoPersist        (base persistance — abstract parent of CNeo*+VC*)
//   #2  CNeoPartMgr        (partition/component manager)
//   #3  CNeoLongIndex      (B-tree index — operates on 0xC2/0xC3 pages)
//   #10 CNeoFileLocation   (filesystem abstraction)
//   #25 CNeoIDList         (ID-list container — fourcc 'ID  ')
//
// These bootstrap before any VC* class registration. Calling
// `register_nl_system_classes()` is mandatory before instantiating VC*
// readers that rely on parent_factory inheritance.

#ifndef NL_SYSTEM_CLASSES_H
#define NL_SYSTEM_CLASSES_H

#include "nl/class_meta.h"

namespace xfiles {
namespace nl {

/// NL system class IDs (from D1 dynamic trace + the format notes).
/// These IDs are used internally by the engine ; user VC* classes start at 0x27.
namespace SystemClassId {
constexpr uint32_t CNeoPersist      = 0x01;  ///< base abstract class
constexpr uint32_t CNeoPartMgr      = 0x02;
constexpr uint32_t CNeoLongIndex    = 0x03;  ///< B-tree index walker
constexpr uint32_t CNeoFileLocation = 0x0a;  ///< (placeholder, exact id TBD)
constexpr uint32_t CNeoIDList       = 0x0b;  ///< from W14 schema
}  // namespace SystemClassId

/// Register the 5 NL system classes in the global ClassRegistry.
///
/// Idempotent : calling twice replaces existing entries (the on-disk format refcount
/// semantics inside already handle this).
void register_nl_system_classes();

/// Register all 51 VC* user classes in the global ClassRegistry.
/// Class IDs + names + factory from the NeoPersist 3.0 class registry
/// (extracted by `scripts/find_vc_constructors.py`).
///
/// In the on-disk format this is the body of `the master class-registration routine` (per D1 boot
/// set, entries #29..#79). In the reference implementation, factory pointers are stubs ;
/// future sessions wire real factories from `src/vc/vc_factory.cpp`.
void register_vc_user_classes();

}  // namespace nl
}  // namespace xfiles

#endif  // NL_SYSTEM_CLASSES_H
