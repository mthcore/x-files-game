// SPDX-License-Identifier: MIT
// Scene graph class Read bodies — VCView, VCNode, VCLocaton.

#include "nl/classes/vc_scene_graph.h"
#include "nl/hdb_serializer.h"
#include "nl/vc_factory.h"

#include <cstring>

namespace xfiles {
namespace nl {

static constexpr uint32_t kFourccNPlt = 0x4e506c74u;
static constexpr uint32_t kFourccNPTc = 0x4e505463u;

// ---------------------------------------------------------------------------
// VCView
// ---------------------------------------------------------------------------
VCView::VCView() : VCObject(), fields_() {
    std::memset(&fields_, 0, sizeof(fields_));
}
void VCView::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    fields_.e_at_0x28 = ser->read_dword(0x65);
    fields_.f_at_0x2c = ser->read_dword(0x66);
    fields_.g_at_0x30 = ser->read_dword(0x67);
    fields_.h_at_0x34 = ser->read_dword(0x68);
    fields_.i_at_0x38 = ser->read_dword(0x69);
    fields_.j_at_0x3c = ser->read_dword(0x6a);
}

// ---------------------------------------------------------------------------
// VCNode
//   base +(ser, 0x65) + read_dword 'f' at +0x64 + read_dword 'g' at +0x68
// ---------------------------------------------------------------------------
VCNode::VCNode() : VCObject() {}
void VCNode::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    children_ = read_obj_list(ser, kFourccNPlt, kFourccNPTc);
    if (ser->error()) return;
    f_ = ser->read_dword(0x66);
    g_ = ser->read_dword(0x67);
}

// ---------------------------------------------------------------------------
// VCLocaton
//   base +(ser, 0x65) + 3 dwords ('f','g','h') + 1 byte ('i')
// ---------------------------------------------------------------------------
VCLocaton::VCLocaton() : VCObject() {}
void VCLocaton::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    children_ = read_obj_list(ser, kFourccNPlt, kFourccNPTc);
    if (ser->error()) return;
    f_ = ser->read_dword(0x66);
    g_ = ser->read_dword(0x67);
    h_ = ser->read_dword(0x68);
    i_ = ser->read_byte(0x69);
}

// --- Write mirrors -------------------------------------------------------
void VCView::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    ser->write_dword(0x65, fields_.e_at_0x28);
    ser->write_dword(0x66, fields_.f_at_0x2c);
    ser->write_dword(0x67, fields_.g_at_0x30);
    ser->write_dword(0x68, fields_.h_at_0x34);
    ser->write_dword(0x69, fields_.i_at_0x38);
    ser->write_dword(0x6a, fields_.j_at_0x3c);
}
void VCNode::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj_list(ser, kFourccNPlt, kFourccNPTc, children_);
    ser->write_dword(0x66, f_);
    ser->write_dword(0x67, g_);
}
void VCLocaton::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj_list(ser, kFourccNPlt, kFourccNPTc, children_);
    ser->write_dword(0x66, f_);
    ser->write_dword(0x67, g_);
    ser->write_dword(0x68, h_);
    ser->write_byte (0x69, i_);
}

}  // namespace nl
}  // namespace xfiles
