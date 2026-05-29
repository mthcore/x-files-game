// SPDX-License-Identifier: MIT
// Remaining VC classes — VCTitle (master), VCDefaultCursors,
// VCDefaultHotSpots, VCTrigger, VCPhoto.

#ifndef NL_CLASSES_VC_REMAINING_H
#define NL_CLASSES_VC_REMAINING_H

#include "nl/vc_object.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace xfiles {
namespace nl {

// ---------------------------------------------------------------------------
// VCTitle — class_id 0x27, master container at the root of the scene graph.
// Source : . Body :
//   base init + if V>1: list iter 'e' + 4 inline sub-objs (f,h,g,i) +
//   2 dwords (j,k) + 2 shorts (l,m).
// ---------------------------------------------------------------------------
class VCTitle : public VCObject {
public:
    VCTitle();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const std::vector<std::unique_ptr<VCObject>>& children() const { return children_; }
    const VCObject* sub_f() const { return sub_f_.get(); }
    const VCObject* sub_g() const { return sub_g_.get(); }
    const VCObject* sub_h() const { return sub_h_.get(); }
    const VCObject* sub_i() const { return sub_i_.get(); }
    uint32_t j() const { return j_; }
    uint32_t k() const { return k_; }
    int32_t  l() const { return l_; }
    int32_t  m() const { return m_; }
    static constexpr uint32_t kClassId = 0x27;
private:
    std::vector<std::unique_ptr<VCObject>> children_;
    std::unique_ptr<VCObject> sub_f_, sub_g_, sub_h_, sub_i_;
    uint32_t j_ = 0, k_ = 0;
    int32_t  l_ = 0, m_ = 0;
};

// ---------------------------------------------------------------------------
// VCDefaultCursors / VCDefaultHotSpots — classes 0x3e / 0x40.
// On-disk Read ( /) walks an inline sub-collection
// at +0x28 reading items with sequential tags starting at 0x65. Our reference implementation
// approximates with the polymorphic list reader.
// ---------------------------------------------------------------------------
class VCDefaultCursors : public VCObject {
public:
    VCDefaultCursors();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const std::vector<std::unique_ptr<VCObject>>& items() const { return items_; }
    static constexpr uint32_t kClassId = 0x3e;
private:
    std::vector<std::unique_ptr<VCObject>> items_;
};

class VCDefaultHotSpots : public VCObject {
public:
    VCDefaultHotSpots();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const std::vector<std::unique_ptr<VCObject>>& items() const { return items_; }
    static constexpr uint32_t kClassId = 0x40;
private:
    std::vector<std::unique_ptr<VCObject>> items_;
};

// ---------------------------------------------------------------------------
// VCTrigger — class_id 0x51, sizeof 0xec (236 B). On-disk Read 
// is essentially a no-op (`return param_2`) — the slot 12 in the vtable is
// "inherited" per vc_slot_map.csv. The actual deserialization is driven by
// the parent class chain (VCObject base). The 3 sub-collections at +0x28,
// +0x5C, +0xA0 are constructed by the factory ctor, not by Read.
// We model it minimally : base + that's it.
// ---------------------------------------------------------------------------
class VCTrigger : public VCObject {
public:
    VCTrigger();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;  // = VCObject::Read only
    static constexpr uint32_t kClassId = 0x51;
};

// ---------------------------------------------------------------------------
// VCName — class_id 0x2f, base + 2 sub-objs (e at +0x28, f at +0x38)
class VCName : public VCObject {
public:
    VCName();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const VCObject* sub_e() const { return sub_e_.get(); }
    const VCObject* sub_f() const { return sub_f_.get(); }
    static constexpr uint32_t kClassId = 0x2f;
private:
    std::unique_ptr<VCObject> sub_e_, sub_f_;
};

// VCCursor — class_id 0x3d, base + sub 'e' + 2 dwords (f,g)
class VCCursor : public VCObject {
public:
    VCCursor();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const VCObject* sub_e() const { return sub_e_.get(); }
    uint32_t f() const { return f_; }
    uint32_t g() const { return g_; }
    static constexpr uint32_t kClassId = 0x3d;
private:
    std::unique_ptr<VCObject> sub_e_;
    uint32_t f_ = 0, g_ = 0;
};

// VCVariable — class_id 0x53, base + sub 'e' + dword_alt 'g' + dword 'i' + byte 'h' + byte 'f'
class VCVariable : public VCObject {
public:
    VCVariable();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const VCObject* sub_e() const { return sub_e_.get(); }
    uint32_t g() const { return g_; }
    uint32_t i() const { return i_; }
    uint8_t  h() const { return h_; }
    uint8_t  f_byte() const { return f_; }
    static constexpr uint32_t kClassId = 0x53;
private:
    std::unique_ptr<VCObject> sub_e_;
    uint32_t g_ = 0, i_ = 0;
    uint8_t  h_ = 0, f_ = 0;
};

// ---------------------------------------------------------------------------
// VCPhoto — class_id 0x59, sizeof 0x78 (120 B). Read = 
// Body : base + sub-obj 'e' + dword 'f' + dword_alt 'g' + inline-sub 'h' +
//        byte 'i' + byte 'j'.
// ---------------------------------------------------------------------------
class VCPhoto : public VCObject {
public:
    VCPhoto();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const VCObject* sub_e() const { return sub_e_.get(); }
    uint32_t f() const { return f_; }
    uint32_t g() const { return g_; }
    const VCObject* sub_h() const { return sub_h_.get(); }
    uint8_t  i() const { return i_; }
    uint8_t  j() const { return j_; }
    static constexpr uint32_t kClassId = 0x59;
private:
    std::unique_ptr<VCObject> sub_e_;
    uint32_t f_ = 0, g_ = 0;
    std::unique_ptr<VCObject> sub_h_;
    uint8_t  i_ = 0, j_ = 0;
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASSES_VC_REMAINING_H
