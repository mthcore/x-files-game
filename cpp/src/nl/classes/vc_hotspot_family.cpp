// SPDX-License-Identifier: MIT
// VCHotSpot family Read bodies.

#include "nl/classes/vc_hotspot_family.h"
#include "nl/hdb_serializer.h"
#include "nl/vc_factory.h"

namespace xfiles {
namespace nl {

static constexpr uint32_t kFourccNPTc = 0x4e505463u;

// VCAssetRef : base + sub-obj 'e' + 2 dwords + 2 bytes
VCAssetRef::VCAssetRef() : VCObject() {}
void VCAssetRef::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    sub_e_ = read_obj(ser, kFourccNPTc);
    if (ser->error()) return;
    f_ = ser->read_dword(0x66);
    g_ = ser->read_dword(0x67);
    h_ = ser->read_byte(0x68);
    i_ = ser->read_byte(0x69);
}

// VCExplorable : base + sub-obj 'e' + 1 dword 'f'
VCExplorable::VCExplorable() : VCObject() {}
void VCExplorable::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    sub_e_ = read_obj(ser, kFourccNPTc);
    if (ser->error()) return;
    f_ = ser->read_dword(0x66);
}

// VCHotSpot : base + sub-obj 'e' + 1 dword 'f'
VCHotSpot::VCHotSpot() : VCObject() {}
void VCHotSpot::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    sub_e_ = read_obj(ser, kFourccNPTc);
    if (ser->error()) return;
    f_ = ser->read_dword(0x66);
}

// --- Write mirrors ---
void VCAssetRef::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj(ser, kFourccNPTc, sub_e_.get());
    ser->write_dword(0x66, f_);
    ser->write_dword(0x67, g_);
    ser->write_byte (0x68, h_);
    ser->write_byte (0x69, i_);
}
void VCExplorable::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj(ser, kFourccNPTc, sub_e_.get());
    ser->write_dword(0x66, f_);
}
void VCHotSpot::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj(ser, kFourccNPTc, sub_e_.get());
    ser->write_dword(0x66, f_);
}

}  // namespace nl
}  // namespace xfiles
