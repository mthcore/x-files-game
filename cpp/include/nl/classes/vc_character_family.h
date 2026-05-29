// SPDX-License-Identifier: MIT
// VC Character family — simple value-holders sharing 1-2 fields.
//
//   VCCharacter (0x2c,) : base + dword 'e'
//   VCPlayer    (0x44,) : thunks to the shared Icon reader (same as VCCharacter)
//   VCIdeaMap   (0x4c,) : base + 2 dwords (e at +0x28, f at +0x2c)

#ifndef NL_CLASSES_VC_CHARACTER_FAMILY_H
#define NL_CLASSES_VC_CHARACTER_FAMILY_H

#include "nl/vc_object.h"

#include <cstdint>

namespace xfiles {
namespace nl {

class VCCharacter : public VCObject {
public:
    VCCharacter();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    uint32_t e_at_0x28() const { return e_; }
    static constexpr uint32_t kClassId = 0x2c;
private:
    uint32_t e_ = 0;
};

class VCPlayer : public VCObject {
public:
    VCPlayer();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    uint32_t e_at_0x28() const { return e_; }
    static constexpr uint32_t kClassId = 0x44;
private:
    uint32_t e_ = 0;
};

class VCIdeaMap : public VCObject {
public:
    VCIdeaMap();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    uint32_t e_at_0x28() const { return e_; }
    uint32_t f_at_0x2c() const { return f_; }
    static constexpr uint32_t kClassId = 0x4c;
private:
    uint32_t e_ = 0, f_ = 0;
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASSES_VC_CHARACTER_FAMILY_H
