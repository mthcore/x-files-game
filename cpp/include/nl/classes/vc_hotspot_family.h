// SPDX-License-Identifier: MIT
// VCHotSpot family — VCAssetRef (0x35), VCExplorable (0x37),
// VCHotSpot (0x39). All share : base + 1 inline polymorphic sub-obj 'e'
// at +0x28 + N direct fields.
//
// On-disk Reads :
//   VCAssetRef    sub + 2 dword + 2 byte (5 fields)
//   VCExplorable  sub + 1 dword
//   VCHotSpot     sub + 1 dword
//
// API-iso : `std::unique_ptr<VCObject> sub_e_` for the polymorphic
// sub-object (the on-disk uses inline vtable dispatch at +0x28).

#ifndef NL_CLASSES_VC_HOTSPOT_FAMILY_H
#define NL_CLASSES_VC_HOTSPOT_FAMILY_H

#include "nl/vc_object.h"

#include <cstdint>
#include <memory>

namespace xfiles {
namespace nl {

class VCAssetRef : public VCObject {
public:
    VCAssetRef();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const VCObject* sub_e() const { return sub_e_.get(); }
    uint32_t f() const { return f_; }
    uint32_t g() const { return g_; }
    uint8_t  h() const { return h_; }
    uint8_t  i() const { return i_; }
    static constexpr uint32_t kClassId = 0x35;
private:
    std::unique_ptr<VCObject> sub_e_;
    uint32_t f_ = 0, g_ = 0;
    uint8_t  h_ = 0, i_ = 0;
};

class VCExplorable : public VCObject {
public:
    VCExplorable();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const VCObject* sub_e() const { return sub_e_.get(); }
    uint32_t f() const { return f_; }
    static constexpr uint32_t kClassId = 0x37;
private:
    std::unique_ptr<VCObject> sub_e_;
    uint32_t f_ = 0;
};

class VCHotSpot : public VCObject {
public:
    VCHotSpot();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const VCObject* sub_e() const { return sub_e_.get(); }
    uint32_t f() const { return f_; }
    static constexpr uint32_t kClassId = 0x39;
private:
    std::unique_ptr<VCObject> sub_e_;
    uint32_t f_ = 0;
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASSES_VC_HOTSPOT_FAMILY_H
