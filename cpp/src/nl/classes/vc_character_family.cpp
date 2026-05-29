// SPDX-License-Identifier: MIT
// VCCharacter family Read bodies (all simple base + N dwords).

#include "nl/classes/vc_character_family.h"
#include "nl/hdb_serializer.h"

namespace xfiles {
namespace nl {

// VCCharacter : base + dword 'e'
VCCharacter::VCCharacter() : VCObject() {}
void VCCharacter::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    e_ = ser->read_dword(0x65);
}

// VCPlayer : thunks to the shared Icon reader — same Read body
VCPlayer::VCPlayer() : VCObject() {}
void VCPlayer::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    e_ = ser->read_dword(0x65);
}

// VCIdeaMap : base + 2 dwords
VCIdeaMap::VCIdeaMap() : VCObject() {}
void VCIdeaMap::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;
    e_ = ser->read_dword(0x65);
    f_ = ser->read_dword(0x66);
}

// Write mirrors
void VCCharacter::Write(HDBSerializer* ser) const {
    VCObject::Write(ser); ser->write_dword(0x65, e_);
}
void VCPlayer::Write(HDBSerializer* ser) const {
    VCObject::Write(ser); ser->write_dword(0x65, e_);
}
void VCIdeaMap::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    ser->write_dword(0x65, e_);
    ser->write_dword(0x66, f_);
}

}  // namespace nl
}  // namespace xfiles
