// SPDX-License-Identifier: MIT
// VC icon-family Read — shared reader for VCInventory / VCActionIcon /
// VCEmotionIcon / VCEvidenceIcon / VCIdeaIcon (and any other 68-byte
// "icon" class that thunks to the shared Icon reader in the on-disk format).
//
// All these classes share the same on-disk layout :
//   +0x00..0x27 : VCObjectBase (40 bytes — vtable, NPfl, vers, flags, refcount)
//   +0x28 'e'   uint32  (always)
//   +0x2c 'f'   uint32  (always)
//   +0x30 'g'   uint32  (always)
//   +0x34 'h'   uint32  (always)
//   +0x38 'k'   uint32  (V > 3 only, default 0)
//   +0x3c 'i'   uint32  (always)
//   +0x40 'j'   uint32  (always)
//   total : 0x44 (68 bytes)
//
// On-disk Read sequence (the on-disk Read sequence) reads in this exact
// order : e, f, g, h, i, j, then k (V>3). The reference implementation matches the same
// call order through HDBSerializer::read_dword.
//
// Per-class thunks observed in the format (vc_factories.csv) :
//   VCInventory      0x4b → Read  →
//   VCActionIcon     0x47 → Read  →
//   VCEvidenceIcon   0x49 → Read  →
//   VCEmotionIcon    0x46 → Read  →
//   VCIdeaIcon       0x48 → Read  →

#ifndef NL_CLASSES_VC_ICON_FAMILY_H
#define NL_CLASSES_VC_ICON_FAMILY_H

#include "nl/vc_object.h"

#include <cstdint>

namespace xfiles {
namespace nl {

/// 7 dword fields appended after the 40-byte VCObject base.
/// Total class size : 68 bytes (0x44).
struct VCIconFamilyFields {
    uint32_t e_at_0x28;
    uint32_t f_at_0x2c;
    uint32_t g_at_0x30;
    uint32_t h_at_0x34;
    uint32_t k_at_0x38;  // optional (V > 3)
    uint32_t i_at_0x3c;
    uint32_t j_at_0x40;
};

/// Byte-direct Read decoder — the shared Read used by every
/// icon-family class. Call this from each subclass's Read after
/// invoking VCObject::Read for the base init.
///
/// `fields` points at the start of the 7-dword block (offset +0x28 of
/// the containing class). The caller has already done VCObject::Read on
/// the same serializer.
void read_icon_family_fields(VCIconFamilyFields& fields,
                              HDBSerializer* ser);

/// Mirror Write for the icon family.
void write_icon_family_fields(const VCIconFamilyFields& fields,
                               HDBSerializer* ser);

// ---------------------------------------------------------------------------
// Per-class concrete types : each is just VCObject + VCIconFamilyFields.
// They differ only by_class_id (registered in system_classes.cpp).
// ---------------------------------------------------------------------------

class VCInventory : public VCObject {
public:
    VCInventory();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const VCIconFamilyFields& fields() const { return fields_; }
    VCIconFamilyFields&       fields()       { return fields_; }
    static constexpr uint32_t kClassId = 0x4b;
private:
    VCIconFamilyFields fields_;
};
// canonical class_id: VCInventory=0x4b ✓

class VCActionIcon : public VCObject {
public:
    VCActionIcon();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const VCIconFamilyFields& fields() const { return fields_; }
    VCIconFamilyFields&       fields()       { return fields_; }
    static constexpr uint32_t kClassId = 0x47;
private:
    VCIconFamilyFields fields_;
};

class VCEvidenceIcon : public VCObject {
public:
    VCEvidenceIcon();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const VCIconFamilyFields& fields() const { return fields_; }
    VCIconFamilyFields&       fields()       { return fields_; }
    static constexpr uint32_t kClassId = 0x49;
private:
    VCIconFamilyFields fields_;
};

class VCEmotionIcon : public VCObject {
public:
    VCEmotionIcon();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const VCIconFamilyFields& fields() const { return fields_; }
    VCIconFamilyFields&       fields()       { return fields_; }
    static constexpr uint32_t kClassId = 0x48;  // canonical VCEmotionIcon
private:
    VCIconFamilyFields fields_;
};

class VCIdeaIcon : public VCObject {
public:
    VCIdeaIcon();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const VCIconFamilyFields& fields() const { return fields_; }
    VCIconFamilyFields&       fields()       { return fields_; }
    static constexpr uint32_t kClassId = 0x4a;  // canonical VCIdeaIcon
private:
    VCIconFamilyFields fields_;
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASSES_VC_ICON_FAMILY_H
