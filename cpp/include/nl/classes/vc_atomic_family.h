// SPDX-License-Identifier: MIT
// VC atomic family — value-holder classes with simple Read patterns.
//
//   VCAssetCategory  (0x3c,) : base + dword 'e' + byte 'f'
//   VCEnabled        (0x46,) : base + dword 'e' + byte 'f' + byte 'g'
//   VCStdAction      (0x54,) : base + 3 dwords (e,f,h)
//   VCString         (0x50,) : base + 1 sub-obj 'e'
//
// All trivially portable byte-direct (no version gates).

#ifndef NL_CLASSES_VC_ATOMIC_FAMILY_H
#define NL_CLASSES_VC_ATOMIC_FAMILY_H

#include "nl/vc_object.h"

#include <cstdint>
#include <memory>

namespace xfiles {
namespace nl {

class VCAssetCategory : public VCObject {
public:
    VCAssetCategory();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    uint32_t e_at_0x28() const { return e_; }
    uint8_t  f_at_0x2c() const { return f_; }
    static constexpr uint32_t kClassId = 0x3c;
private:
    uint32_t e_ = 0;
    uint8_t  f_ = 0;
};

class VCEnabled : public VCObject {
public:
    VCEnabled();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    uint32_t e_at_0x28() const { return e_; }
    uint8_t  g_at_0x2c() const { return g_; }
    uint8_t  f_at_0x2d() const { return f_; }
    static constexpr uint32_t kClassId = 0x46;  // canonical VCEnabled
private:
    uint32_t e_ = 0;
    uint8_t  g_ = 0, f_ = 0;
};

class VCStdAction : public VCObject {
public:
    VCStdAction();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    uint32_t e_at_0x28() const { return e_; }
    uint32_t f_at_0x2c() const { return f_; }
    uint32_t h_at_0x30() const { return h_; }
    static constexpr uint32_t kClassId = 0x54;
private:
    uint32_t e_ = 0, f_ = 0, h_ = 0;
};

class VCString : public VCObject {
public:
    VCString();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const VCObject* sub_e() const { return sub_e_.get(); }
    static constexpr uint32_t kClassId = 0x50;  // canonical VCString
private:
    std::unique_ptr<VCObject> sub_e_;
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASSES_VC_ATOMIC_FAMILY_H
