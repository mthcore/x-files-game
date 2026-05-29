// SPDX-License-Identifier: MIT
// Icon-family shared Read — byte-direct Read decoder.
//
// Side-by-side with the on-disk Read sequence :
//
//   On-disk read order                  C++ reader (this file)
//   ---------------                              ------------------
// (done by caller : VCObject::Read)
//   this+0x28 = pcVar2(0x65);                    fields.e_at_0x28 = ser->read_dword(0x65);
//   this+0x2c = pcVar2(0x66);                    fields.f_at_0x2c = ser->read_dword(0x66);
//   this+0x30 = pcVar2(0x67);                    fields.g_at_0x30 = ser->read_dword(0x67);
//   this+0x34 = pcVar2(0x68);                    fields.h_at_0x34 = ser->read_dword(0x68);
//   this+0x3c = pcVar2(0x69);                    fields.i_at_0x3c = ser->read_dword(0x69);
//   this+0x40 = pcVar2(0x6a);                    fields.j_at_0x40 = ser->read_dword(0x6a);
//   if (3 < *(uint*)(iVar1 + 4)) {               if (ser->current_version() > 3u) {
//     this+0x38 = pcVar2(0x6b);                    fields.k_at_0x38 = ser->read_dword(0x6b);
//   }                                            }

#include "nl/classes/vc_icon_family.h"

#include <cstddef>
#include <cstring>
#include "vc/_size_assert.h"

namespace xfiles {
namespace nl {

// Compile-time check : the 7-field block is exactly 28 bytes (0x1c), so
// VCObjectBase (0x28) + VCIconFamilyFields (0x1c) = 0x44 = 68 bytes total.
XFILES_ASSERT_SIZE(VCIconFamilyFields, 0x1c, "VCIconFamilyFields must be 28 bytes (7 × uint32)");

void read_icon_family_fields(VCIconFamilyFields& fields, HDBSerializer* ser) {
    if (!ser || ser->error()) return;

    // Always-present fields (e, f, g, h, i, j) — read in on-disk order.
    fields.e_at_0x28 = ser->read_dword(0x65);
    fields.f_at_0x2c = ser->read_dword(0x66);
    fields.g_at_0x30 = ser->read_dword(0x67);
    fields.h_at_0x34 = ser->read_dword(0x68);
    fields.i_at_0x3c = ser->read_dword(0x69);
    fields.j_at_0x40 = ser->read_dword(0x6a);

    // Version-gated field (V > 3) : 'k' at +0x38
    if (ser->current_version() > 3u) {
        fields.k_at_0x38 = ser->read_dword(0x6b);
    }
}

void write_icon_family_fields(const VCIconFamilyFields& fields,
                               HDBSerializer* ser) {
    if (!ser) return;
    ser->write_dword(0x65, fields.e_at_0x28);
    ser->write_dword(0x66, fields.f_at_0x2c);
    ser->write_dword(0x67, fields.g_at_0x30);
    ser->write_dword(0x68, fields.h_at_0x34);
    ser->write_dword(0x69, fields.i_at_0x3c);
    ser->write_dword(0x6a, fields.j_at_0x40);
    if (ser->current_version() > 3u) {
        ser->write_dword(0x6b, fields.k_at_0x38);
    }
}

// ---------------------------------------------------------------------------
// Per-class boilerplate. Each class just wires its own VCObject::Read +
// the shared icon-family reader.
// ---------------------------------------------------------------------------

VCInventory::VCInventory()    : VCObject(), fields_() {
    std::memset(&fields_, 0, sizeof(fields_));
}
void VCInventory::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    read_icon_family_fields(fields_, ser);
}
void VCInventory::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_icon_family_fields(fields_, ser);
}

VCActionIcon::VCActionIcon()  : VCObject(), fields_() {
    std::memset(&fields_, 0, sizeof(fields_));
}
void VCActionIcon::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    read_icon_family_fields(fields_, ser);
}
void VCActionIcon::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_icon_family_fields(fields_, ser);
}

VCEvidenceIcon::VCEvidenceIcon() : VCObject(), fields_() {
    std::memset(&fields_, 0, sizeof(fields_));
}
void VCEvidenceIcon::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    read_icon_family_fields(fields_, ser);
}
void VCEvidenceIcon::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_icon_family_fields(fields_, ser);
}

VCEmotionIcon::VCEmotionIcon() : VCObject(), fields_() {
    std::memset(&fields_, 0, sizeof(fields_));
}
void VCEmotionIcon::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    read_icon_family_fields(fields_, ser);
}
void VCEmotionIcon::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_icon_family_fields(fields_, ser);
}

VCIdeaIcon::VCIdeaIcon() : VCObject(), fields_() {
    std::memset(&fields_, 0, sizeof(fields_));
}
void VCIdeaIcon::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    read_icon_family_fields(fields_, ser);
}
void VCIdeaIcon::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_icon_family_fields(fields_, ser);
}

}  // namespace nl
}  // namespace xfiles
