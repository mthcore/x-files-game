// SPDX-License-Identifier: MIT
// VCUI family Read bodies.

#include "nl/classes/vc_ui_family.h"
#include "nl/hdb_serializer.h"
#include "nl/vc_factory.h"

namespace xfiles {
namespace nl {

static constexpr uint32_t kFourccNPlt = 0x4e506c74u;
static constexpr uint32_t kFourccNPTc = 0x4e505463u;

// VCIFaceLayout
VCIFaceLayout::VCIFaceLayout() : VCObject() {}
void VCIFaceLayout::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    items_ = read_obj_list(ser, kFourccNPlt, kFourccNPTc);
    if (ser->error()) return;
    f_ = ser->read_dword(0x66);
    g_ = ser->read_byte(0x67);
}

// VCInterfaceItem
VCInterfaceItem::VCInterfaceItem() : VCObject() {}
void VCInterfaceItem::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    sub_e_ = read_obj(ser, kFourccNPTc);
    if (ser->error()) return;
    f_ = ser->read_dword(0x66);
    g_ = ser->read_dword(0x67);
    // The on-disk format uses read slot 0xc0 for 'h' (word read alt) — we approximate
    // with read_short (slot 0xb0) since our serializer doesn't expose 0xc0.
    h_ = static_cast<uint16_t>(ser->read_short(0x68));
    if (ser->current_version() > 4u) {
        i_ = ser->read_short(0x69);
    }
}

// VCViewPoint
VCViewPoint::VCViewPoint() : VCObject() {}
void VCViewPoint::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    items_ = read_obj_list(ser, kFourccNPlt, kFourccNPTc);
    if (ser->error()) return;
    sub_f_ = read_obj(ser, kFourccNPTc);
    if (ser->error()) return;
    g_ = ser->read_dword(0x67);
    h_ = ser->read_dword(0x68);
}

// VCCharView — base + dword 'e' + inline-sub 'f' + 4 dwords
VCCharView::VCCharView() : VCObject() {}
void VCCharView::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    e_ = ser->read_dword(0x65);
    if (ser->error()) return;
    sub_f_ = read_obj(ser, kFourccNPTc);
    if (ser->error()) return;
    g_ = ser->read_dword(0x67);
    h_ = ser->read_dword(0x68);
    i_ = ser->read_dword(0x69);
    j_ = ser->read_dword(0x6a);
}

// --- Write mirrors -----------------------------------------------------
void VCIFaceLayout::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj_list(ser, kFourccNPlt, kFourccNPTc, items_);
    ser->write_dword(0x66, f_);
    ser->write_byte (0x67, g_);
}
void VCInterfaceItem::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj(ser, kFourccNPTc, sub_e_.get());
    ser->write_dword(0x66, f_);
    ser->write_dword(0x67, g_);
    ser->write_short(0x68, static_cast<int32_t>(static_cast<int16_t>(h_)));
    if (ser->current_version() > 4u) {
        ser->write_short(0x69, i_);
    }
}
void VCViewPoint::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj_list(ser, kFourccNPlt, kFourccNPTc, items_);
    write_obj(ser, kFourccNPTc, sub_f_.get());
    ser->write_dword(0x67, g_);
    ser->write_dword(0x68, h_);
}
void VCCharView::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    ser->write_dword(0x65, e_);
    write_obj(ser, kFourccNPTc, sub_f_.get());
    ser->write_dword(0x67, g_);
    ser->write_dword(0x68, h_);
    ser->write_dword(0x69, i_);
    ser->write_dword(0x6a, j_);
}

}  // namespace nl
}  // namespace xfiles
