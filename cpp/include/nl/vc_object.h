// SPDX-License-Identifier: MIT
// VCObject / CNeoPersist — 40-byte runtime base for every VC* class.
//
// In the on-disk format, all 51 VC* classes (VCTitle 0x27 … VCGameState 0x57)
// inherit from VCObject (itself a CNeoPersist subclass). The combined base
// layout is 40 bytes (0x28) :
//
//   +0x00 : vtable*                          (4B)
//   +0x04 : field_0x04                       (4B — class_meta_ptr ?)
//   +0x08 : npfl_flags                       (2B — read by base init,
//                                                  fourcc 'NPfl')
//   +0x0a : 6B padding / further inherited
//   +0x10 : flag_byte                        (1B — bits 5-6 set, bits 0-1
//                                                  cleared by base init)
//   +0x11 : 3B padding
//   +0x14 : flag_dword                       (4B — low nibble cleared by
//                                                  base init)
//   +0x18 : vers                              (4B — version, read by base
//                                                  init fourcc 'vers')
//   +0x1c : field_0x1c                       (4B)
//   +0x20 : cache_dword                      (4B — cleared by slot15)
//   +0x24 : refcount                          (2B — set to 1 by base init,
//                                                  incremented by slot25)
//   +0x26 : field_0x26                       (2B)
//
//  Total : 40 bytes (sizeof base = 0x28). All 51 VC* subclasses begin
//  their own fields at offset +0x28. Confirmed empirically by W7 layout
//  audit (the format notes).
//
// `VCObject::Read(HDBSerializer*)` is the byte-direct port of
// the on-disk base reader. Each subclass's Read
// invokes this base reader first, then pulls its own fields by tag.
//
// Reference :
//   - the format notes (28-slot vtable contract)
//   - the on-disk base read sequence

#ifndef NL_VC_OBJECT_H
#define NL_VC_OBJECT_H

#include "nl/hdb_serializer.h"

#include <cstdint>

namespace xfiles {
namespace nl {

/// Fourcc constants used by the base reader, in numeric (big-endian-
/// assembled) form to match the format's fixed constants.
constexpr uint32_t k_fourcc_npfl = 0x4e50666cu;  // 'NPfl'
constexpr uint32_t k_fourcc_vers = 0x76657273u;  // 'vers'

/// 40-byte base, mirrored 1:1 from the on-disk layout. Padding
/// inside the struct is named so each named field lands at the exact
/// offset documented above.
///
/// NOT packed — we rely on the natural alignment of int32 / int16 to
/// match the on-disk format's MSVC 4.2 default layout. A static_assert on
/// sizeof / offsetof enforces the contract at compile time.
struct VCObjectBase {
    void*     vtable;        // +0x00
    uint32_t  field_0x04;    // +0x04
    uint16_t  npfl_flags;    // +0x08
    uint8_t   _pad_0x0a[6];  // +0x0a..+0x0f
    uint8_t   flag_byte;     // +0x10
    uint8_t   _pad_0x11[3];  // +0x11..+0x13
    uint32_t  flag_dword;    // +0x14
    uint32_t  vers;          // +0x18
    uint32_t  field_0x1c;    // +0x1c
    uint32_t  cache_dword;   // +0x20
    int16_t   refcount;      // +0x24
    uint16_t  field_0x26;    // +0x26
};

/// VCObject runtime base class. Holds the 40-byte header layout and
/// exposes Read() = base init from disk. Subclasses derive from this
/// and add their own fields at offset +0x28.
class VCObject {
public:
    VCObject();
    virtual ~VCObject() = default;

    /// Byte-direct Read decoder. Reads :
    ///   1. begin_record (vtable slot 0xa8)
    ///   2. NPfl flags  (vtable slot 0x74, 2 bytes → +0x08)
    ///   3. vers dword  (vtable slot 0xbc, conditional on
    ///                    ser->has_version_field(), → +0x18)
    /// Then sets :
    ///   - flag_byte  = (flag_byte & 0xfc) | 0x60
    ///   - flag_dword = flag_dword & 0xfffffff0
    ///   - refcount   = 1
    virtual void Read(HDBSerializer* ser);

    /// Mirror Write — a later revision. Emits NPfl + vers (when version field
    /// is declared by the class meta). Subclasses chain to base first
    /// then write their own fields in the same order as Read consumed.
    virtual void Write(HDBSerializer* ser) const;

    /// Access the 40-byte base view (test helper + iso checks).
    const VCObjectBase& base() const { return base_; }
    VCObjectBase&       base()       { return base_; }

protected:
    VCObjectBase base_;
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_VC_OBJECT_H
