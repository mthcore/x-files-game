// SPDX-License-Identifier: MIT
// VC UI family — VCIFaceLayout, VCInterfaceItem, VCViewPoint, VCCharView.
// Each has base + (list iter OR inline sub-obj) + 1-4 direct fields.
//
//   VCIFaceLayout (0x4e,)  : base + list 'e' + dword 'f' + byte 'g'
//   VCInterfaceItem (0x4f,): base + sub-obj 'e' + 2 dwords + word + cond short
//   VCViewPoint (0x2a,)    : base + list 'e' + sub-obj 'f' + 2 dwords
//   VCCharView (0x2d,)     : base + dword 'e' + sub-obj 'f' + 4 dwords

#ifndef NL_CLASSES_VC_UI_FAMILY_H
#define NL_CLASSES_VC_UI_FAMILY_H

#include "nl/vc_object.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace xfiles {
namespace nl {

class VCIFaceLayout : public VCObject {
public:
    VCIFaceLayout();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const std::vector<std::unique_ptr<VCObject>>& items() const { return items_; }
    uint32_t f() const { return f_; }
    uint8_t  g() const { return g_; }
    static constexpr uint32_t kClassId = 0x4e;
private:
    std::vector<std::unique_ptr<VCObject>> items_;
    uint32_t f_ = 0;
    uint8_t  g_ = 0;
};

class VCInterfaceItem : public VCObject {
public:
    VCInterfaceItem();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const VCObject* sub_e() const { return sub_e_.get(); }
    uint32_t f_at_0x44() const { return f_; }
    uint32_t g_at_0x48() const { return g_; }
    uint16_t h_at_0x4c() const { return h_; }
    int32_t  i_at_0x50() const { return i_; }  // V>4 only
    static constexpr uint32_t kClassId = 0x4f;
private:
    std::unique_ptr<VCObject> sub_e_;
    uint32_t f_ = 0, g_ = 0;
    uint16_t h_ = 0;
    int32_t  i_ = 0;
};

class VCViewPoint : public VCObject {
public:
    VCViewPoint();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const std::vector<std::unique_ptr<VCObject>>& items() const { return items_; }
    const VCObject* sub_f() const { return sub_f_.get(); }
    uint32_t g_at_0x70() const { return g_; }
    uint32_t h_at_0x74() const { return h_; }
    static constexpr uint32_t kClassId = 0x2a;
private:
    std::vector<std::unique_ptr<VCObject>> items_;
    std::unique_ptr<VCObject> sub_f_;
    uint32_t g_ = 0, h_ = 0;
};

class VCCharView : public VCObject {
public:
    VCCharView();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    uint32_t e_at_0x28() const { return e_; }
    const VCObject* sub_f() const { return sub_f_.get(); }
    uint32_t g() const { return g_; }
    uint32_t h() const { return h_; }
    uint32_t i() const { return i_; }
    uint32_t j() const { return j_; }
    static constexpr uint32_t kClassId = 0x2d;
private:
    uint32_t e_ = 0;
    std::unique_ptr<VCObject> sub_f_;
    uint32_t g_ = 0, h_ = 0, i_ = 0, j_ = 0;
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASSES_VC_UI_FAMILY_H
