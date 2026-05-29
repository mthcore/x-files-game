// SPDX-License-Identifier: MIT
// VC atomic family Read bodies.

#include "nl/classes/vc_atomic_family.h"
#include "nl/hdb_serializer.h"
#include "nl/vc_factory.h"

namespace xfiles {
namespace nl {

static constexpr uint32_t kFourccNPTc = 0x4e505463u;

// VCAssetCategory
VCAssetCategory::VCAssetCategory() : VCObject() {}
void VCAssetCategory::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    e_ = ser->read_dword(0x65);
    f_ = ser->read_byte(0x66);
}

// VCEnabled — note the on-disk format uses tag 'f' for +0x2d, 'g' for +0x2c
VCEnabled::VCEnabled() : VCObject() {}
void VCEnabled::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    e_ = ser->read_dword(0x65);
    f_ = ser->read_byte(0x66);
    g_ = ser->read_byte(0x67);
}

// VCStdAction
VCStdAction::VCStdAction() : VCObject() {}
void VCStdAction::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    e_ = ser->read_dword(0x65);
    f_ = ser->read_dword(0x66);
    h_ = ser->read_dword(0x68);
}

// VCString — only 1 sub-obj
VCString::VCString() : VCObject() {}
void VCString::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    sub_e_ = read_obj(ser, kFourccNPTc);
}

// --- Write mirrors -------------------------------------------------------

void VCAssetCategory::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    ser->write_dword(0x65, e_);
    ser->write_byte (0x66, f_);
}
void VCEnabled::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    ser->write_dword(0x65, e_);
    ser->write_byte (0x66, f_);
    ser->write_byte (0x67, g_);
}
void VCStdAction::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    ser->write_dword(0x65, e_);
    ser->write_dword(0x66, f_);
    ser->write_dword(0x68, h_);
}
void VCString::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    write_obj(ser, kFourccNPTc, sub_e_.get());
}

}  // namespace nl
}  // namespace xfiles
