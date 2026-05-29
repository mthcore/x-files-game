// SPDX-License-Identifier: MIT
// VCObject — base class for VirtualCinema engine object hierarchy.
//
// Byte-direct vtable layout matching the on-disk format (1998, MSVC ~4.2 era).
// 28 virtual slots × 4 bytes = 112-byte vtable per class.
//
// Reference :
//   the format notes — class hierarchy
//   the format notes         — VCObject decoded
//   the on-disk Read sequence        — VCGameState ctor (uses VCObject vtable)
//   notes/W21_31_vcgs_02541_VCGS_Write.bin — live runtime dump of VCGameState (vtable @ +0x00)

#ifndef VC_OBJECT_H
#define VC_OBJECT_H

#include <cstdint>
#include "vc/_size_assert.h"

namespace xfiles {
namespace hdb {
class HDBContext;          // serializer context (HDB stream reader/writer)
class MasterHDB;           // master HDB ID dispatch
class TLVReader;
class TLVWriter;
}  // namespace hdb

namespace vc {

// Convenience using-declaration so existing code can write `HDBContext*`.
using HDBContext = hdb::HDBContext;

/// VCObject — root of VirtualCinema object hierarchy.
///
/// Virtual table layout (28 slots) :
///   slot  0 (+0x00) : dtor      vector deleting destructor (MSVC convention)
///   slot  1 (+0x04) : ?         engine internal
///   slot  2 (+0x08) : ?         engine internal
///   slot  3 (+0x0c) : ?         engine internal
///   slot  4 (+0x10) : ?         engine internal
///   slot  5 (+0x14) : ?         engine internal
///   slot  6 (+0x18) : is_leaf   used by sub-coll dispatch (returns char)
///   slot  7 (+0x1c) : ?
///   slot  8 (+0x20) : ?
///   slot  9 (+0x24) : id_lookup master HDB ID->Object (per W23.4 step 3)
///   slot 10 (+0x28) : ?
///   slot 11 (+0x2c) : eval      universal Eval entry
///   slot 12 (+0x30) : Read      deserialize from HDB stream (per W23.4)
///   slot 13 (+0x34) : Write     serialize to HDB stream
///   slot 14 (+0x38) : ?
///   slot 15 (+0x3c) : ?
///   slot 16 (+0x40) : ?
///   slot 17 (+0x44) : ?
///   slot 18 (+0x48) : ?
///   slot 19 (+0x4c) : ?
///   slot 20 (+0x50) : ?
///   slot 21 (+0x54) : ?
///   slot 22 (+0x58) : ?
///   slot 23 (+0x5c) : ?
///   slot 24 (+0x60) : ?
///   slot 25 (+0x64) : ?
///   slot 26 (+0x68) : ?
///   slot 27 (+0x6c) : ?
///
/// Slots 1-5, 7-8, 10, 14-27 to be filled in P2 as we decode each VC* class's
/// vtable. For now, declared as pure virtual placeholders so derived classes
/// must override them (and the compiler-generated vtable has 28 entries).
class VCObject {
public:
    // Slot 0 : virtual destructor (MSVC generates vector deleting destructor)
    virtual ~VCObject() = default;

    // Slots 1-5 — engine internals TBD (P2)
    virtual void vt_slot_01() = 0;
    virtual void vt_slot_02() = 0;
    virtual void vt_slot_03() = 0;
    virtual void vt_slot_04() = 0;
    virtual void vt_slot_05() = 0;

    // Slot 6 : is_leaf — returns non-zero if leaf object (sub-coll dispatch)
    virtual char is_leaf() const = 0;

    // Slots 7-8 TBD
    virtual void vt_slot_07() = 0;
    virtual void vt_slot_08() = 0;

    // Slot 9 : id_lookup — master HDB ID-to-Object resolver
    virtual VCObject* id_lookup(uint32_t id) = 0;

    // Slot 10 TBD
    virtual void vt_slot_10() = 0;

    // Slot 11 : eval — universal Eval ( in the on-disk format)
    virtual uint32_t eval(uint32_t param_1, uint32_t param_2) = 0;

    // Slot 12 : Read — deserialize from HDB stream
    virtual void Read(HDBContext* ctx, uint32_t param_2) = 0;

    // Slot 13 : Write — serialize to HDB stream
    virtual void Write(HDBContext* ctx) const = 0;

    // Slots 14-27 TBD (P2)
    virtual void vt_slot_14() = 0;
    virtual void vt_slot_15() = 0;
    virtual void vt_slot_16() = 0;
    virtual void vt_slot_17() = 0;
    virtual void vt_slot_18() = 0;
    virtual void vt_slot_19() = 0;
    virtual void vt_slot_20() = 0;
    virtual void vt_slot_21() = 0;
    virtual void vt_slot_22() = 0;
    virtual void vt_slot_23() = 0;
    virtual void vt_slot_24() = 0;
    virtual void vt_slot_25() = 0;
    virtual void vt_slot_26() = 0;
    virtual void vt_slot_27() = 0;
};

// Compile-time verification : vtable pointer occupies first 4 bytes.
// The whole object is 4 bytes (just the vtable pointer) since VCObject has no members.
XFILES_ASSERT_SIZE(VCObject, 4, "VCObject should be 4 bytes (vtable ptr only)");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_OBJECT_H
