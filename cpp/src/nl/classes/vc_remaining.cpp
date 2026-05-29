// SPDX-License-Identifier: MIT
// Remaining VC classes Read bodies.

#include "nl/classes/vc_remaining.h"
#include "nl/hdb_serializer.h"
#include "nl/vc_factory.h"

namespace xfiles {
namespace nl {

static constexpr uint32_t kFourccNPlt = 0x4e506c74u;
static constexpr uint32_t kFourccNPTc = 0x4e505463u;

// VCTitle
VCTitle::VCTitle() : VCObject() {}
void VCTitle::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    if (ser->current_version() > 1u) {
        children_ = read_obj_list(ser, kFourccNPlt, kFourccNPTc);
        if (ser->error()) return;
    }
    sub_f_ = read_obj(ser, kFourccNPTc);
    sub_h_ = read_obj(ser, kFourccNPTc);
    sub_g_ = read_obj(ser, kFourccNPTc);
    sub_i_ = read_obj(ser, kFourccNPTc);
    if (ser->error()) return;
    j_ = ser->read_dword(0x6a);
    k_ = ser->read_dword(0x6b);
    l_ = ser->read_short(0x6c);
    m_ = ser->read_short(0x6d);
}

// VCDefaultCursors — inline sub-coll as polymorphic list
VCDefaultCursors::VCDefaultCursors() : VCObject() {}
void VCDefaultCursors::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    items_ = read_obj_list(ser, kFourccNPlt, kFourccNPTc);
}

// VCDefaultHotSpots
VCDefaultHotSpots::VCDefaultHotSpots() : VCObject() {}
void VCDefaultHotSpots::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    items_ = read_obj_list(ser, kFourccNPlt, kFourccNPTc);
}

// VCTrigger ( == NOP) — just VCObject::Read
VCTrigger::VCTrigger() : VCObject() {}
void VCTrigger::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
}

// VCName : base + 2 sub-objs ('e', 'f')
VCName::VCName() : VCObject() {}
void VCName::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    sub_e_ = read_obj(ser, kFourccNPTc);
    if (ser->error()) return;
    sub_f_ = read_obj(ser, kFourccNPTc);
}

// VCCursor : base + sub-obj 'e' + 2 dwords (f, g)
VCCursor::VCCursor() : VCObject() {}
void VCCursor::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    sub_e_ = read_obj(ser, kFourccNPTc);
    if (ser->error()) return;
    f_ = ser->read_dword(0x66);
    g_ = ser->read_dword(0x67);
}

// VCVariable : base + sub 'e' + dword_alt 'g' + dword 'i'
//                              + byte 'h' + byte 'f'
VCVariable::VCVariable() : VCObject() {}
void VCVariable::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    sub_e_ = read_obj(ser, kFourccNPTc);
    if (ser->error()) return;
    g_ = ser->read_dword(0x67);
    i_ = ser->read_dword(0x69);
    h_ = ser->read_byte(0x68);
    f_ = ser->read_byte(0x66);
}

// VCPhoto
VCPhoto::VCPhoto() : VCObject() {}
void VCPhoto::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    sub_e_ = read_obj(ser, kFourccNPTc);
    if (ser->error()) return;
    f_ = ser->read_dword(0x66);
    g_ = ser->read_dword(0x67);
    if (ser->error()) return;
    sub_h_ = read_obj(ser, kFourccNPTc);
    if (ser->error()) return;
    i_ = ser->read_byte(0x69);
    j_ = ser->read_byte(0x6a);
}

// --- Write mirrors -----------------------------------------------------
void VCTitle::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    if (ser->current_version() > 1u) {
        write_obj_list(ser, kFourccNPlt, kFourccNPTc, children_);
    }
    write_obj(ser, kFourccNPTc, sub_f_.get());
    write_obj(ser, kFourccNPTc, sub_h_.get());
    write_obj(ser, kFourccNPTc, sub_g_.get());
    write_obj(ser, kFourccNPTc, sub_i_.get());
    ser->write_dword(0x6a, j_);
    ser->write_dword(0x6b, k_);
    ser->write_short(0x6c, l_);
    ser->write_short(0x6d, m_);
}
void VCDefaultCursors::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj_list(ser, kFourccNPlt, kFourccNPTc, items_);
}
void VCDefaultHotSpots::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj_list(ser, kFourccNPlt, kFourccNPTc, items_);
}
void VCTrigger::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
}
void VCName::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj(ser, kFourccNPTc, sub_e_.get());
    write_obj(ser, kFourccNPTc, sub_f_.get());
}
void VCCursor::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj(ser, kFourccNPTc, sub_e_.get());
    ser->write_dword(0x66, f_);
    ser->write_dword(0x67, g_);
}
void VCVariable::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj(ser, kFourccNPTc, sub_e_.get());
    ser->write_dword(0x67, g_);
    ser->write_dword(0x69, i_);
    ser->write_byte (0x68, h_);
    ser->write_byte (0x66, f_);
}
void VCPhoto::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj(ser, kFourccNPTc, sub_e_.get());
    ser->write_dword(0x66, f_);
    ser->write_dword(0x67, g_);
    write_obj(ser, kFourccNPTc, sub_h_.get());
    ser->write_byte (0x69, i_);
    ser->write_byte (0x6a, j_);
}

}  // namespace nl
}  // namespace xfiles
