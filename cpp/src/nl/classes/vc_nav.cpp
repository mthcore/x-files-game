// SPDX-License-Identifier: MIT
// VCNav byte-direct port — 
//
// On-disk read order              C++ reader
// ---------------                       ------
// base init -> VCObject::Read(ser);
// this+0x28 = pcVar1(0x65);             fields_.e_at_0x28 = ser->read_dword(0x65);
// this+0x2c = pcVar1(0x66);             fields_.f_at_0x2c = ser->read_dword(0x66);
// this+0x30 = pcVar1(0x67);             fields_.g_at_0x30 = ser->read_dword(0x67);

#include "nl/classes/vc_nav.h"

#include <cstddef>
#include <cstring>

namespace xfiles {
namespace nl {

#if !(defined(_MSC_VER) && !defined(__clang__))
static_assert(sizeof(VCObjectBase) + sizeof(VCNavFields) == VCNav::kSizeOf,
              "VCNav total size must be 0x38 (56 B)");
#endif

VCNav::VCNav() : VCObject(), fields_() {
    std::memset(&fields_, 0, sizeof(fields_));
}

void VCNav::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    fields_.e_at_0x28 = ser->read_dword(0x65);
    fields_.f_at_0x2c = ser->read_dword(0x66);
    fields_.g_at_0x30 = ser->read_dword(0x67);
}

void VCNav::Write(HDBSerializer* ser) const {
    if (!ser) return;
    VCObject::Write(ser);
    ser->write_dword(0x65, fields_.e_at_0x28);
    ser->write_dword(0x66, fields_.f_at_0x2c);
    ser->write_dword(0x67, fields_.g_at_0x30);
}

}  // namespace nl
}  // namespace xfiles
