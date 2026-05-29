// SPDX-License-Identifier: MIT
// VCScene graph family — Title, Node, Locaton, ViewPoint, View, Character,
// CharView. These are the gameplay scene-graph classes. Each has its own
// Read body but they share the "base init + optional list iter + N
// direct fields + optional inline sub-objects" macro pattern.
//
// VCView is the simplest : base + 6 direct dwords.
// Others have additional list iter and inline sub-object
// dispatches that we approximate with sub_e_/sub_h_ unique_ptr fields.

#ifndef NL_CLASSES_VC_SCENE_GRAPH_H
#define NL_CLASSES_VC_SCENE_GRAPH_H

#include "nl/vc_object.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace xfiles {
namespace nl {

// ---------------------------------------------------------------------------
// VCView — class_id 0x2b, sizeof TBD, Read = 
// Layout : VCObject base + 6 uint32 (e,f,g,h,i,j) at +0x28..+0x3c.
// Simplest scene-graph class : no sub-objects, no list, no version gate.
// ---------------------------------------------------------------------------

struct VCViewFields {
    uint32_t e_at_0x28, f_at_0x2c, g_at_0x30, h_at_0x34;
    uint32_t i_at_0x38, j_at_0x3c;
};

class VCView : public VCObject {
public:
    VCView();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const VCViewFields& fields() const { return fields_; }
    static constexpr uint32_t kClassId = 0x2b;
private:
    VCViewFields fields_;
};

// ---------------------------------------------------------------------------
// VCNode — class_id 0x28, Read = 
// Layout : base + list iter 'e' + 2 uint32 ('f' at +100/0x64, 'g' at +0x68).
// API-iso : children_ vector for the list, two direct fields.
// ---------------------------------------------------------------------------

class VCNode : public VCObject {
public:
    VCNode();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const std::vector<std::unique_ptr<VCObject>>& children() const { return children_; }
    uint32_t f_at_0x64() const { return f_; }
    uint32_t g_at_0x68() const { return g_; }
    static constexpr uint32_t kClassId = 0x28;
private:
    std::vector<std::unique_ptr<VCObject>> children_;
    uint32_t f_ = 0, g_ = 0;
};

// ---------------------------------------------------------------------------
// VCLocaton — class_id 0x29, Read = 
// Layout : base + list iter 'e' + 3 uint32 ('f' at +100, 'g' +0x68, 'h' +0x6c)
//          + 1 byte ('i' at +0x74).
// ---------------------------------------------------------------------------

class VCLocaton : public VCObject {
public:
    VCLocaton();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;
    const std::vector<std::unique_ptr<VCObject>>& children() const { return children_; }
    uint32_t f() const { return f_; }
    uint32_t g() const { return g_; }
    uint32_t h() const { return h_; }
    uint8_t  i() const { return i_; }
    static constexpr uint32_t kClassId = 0x29;
private:
    std::vector<std::unique_ptr<VCObject>> children_;
    uint32_t f_ = 0, g_ = 0, h_ = 0;
    uint8_t  i_ = 0;
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASSES_VC_SCENE_GRAPH_H
