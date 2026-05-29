// SPDX-License-Identifier: MIT
// VCConversation byte-direct port — reference implementation.
//
// Side-by-side with the on-disk Read sequence :
//
//   On-disk read order                C++ reader (this file)
//   ---------------                            ------------------
// base init -> VCObject::Read(ser);
//   this+0x28 = pcVar2(0x65);                  fields_.e_at_0x28 = ser->read_dword('e');
//   this+0x2c = pcVar2(0x66);                  fields_.f_at_0x2c = ser->read_dword('f');
//   this+0x34 = pcVar2(0x68);                  fields_.h_at_0x34 = ser->read_dword('h');
//   this+0x3c = pcVar2(0x6a);                  fields_.j_at_0x3c = ser->read_dword('j');
//   if (2 < *(uint*)(iVar1 + 4)) {             if (ser->current_version() > 2) {
//     this+0x30 = pcVar2(0x67);                    fields_.g_at_0x30 = ser->read_dword('g');
//     this+0x38 = pcVar2(0x69);                    fields_.i_at_0x38 = ser->read_dword('i');
//   }                                          }
//   if (3 < *(uint*)(iVar1 + 4)) {             if (ser->current_version() > 3) {
//            (0x6b);                                 ser->read_byte_alt('k');
//     this+0x40 = uVar3;
//   }                                          }
//
// Tag bytes : 'e'=0x65, 'f'=0x66, 'g'=0x67, 'h'=0x68, 'i'=0x69,
//             'j'=0x6a, 'k'=0x6b. Read order : e/f/h/j (always),
//             g/i (V>2), k (V>3). Matches W23_4_hdb_tlv_per_class.md.

#include "nl/classes/vc_conversation.h"

#include <cstddef>
#include <cstring>
#include "vc/_size_assert.h"

namespace xfiles {
namespace nl {

// Sanity : the subclass body offsets must align to on-disk +0x28..+0x40.
XFILES_ASSERT_OFFSET(VCConversationFields, e_at_0x28, 0x00, "e at sub+0x00 == on-disk +0x28");
XFILES_ASSERT_OFFSET(VCConversationFields, f_at_0x2c, 0x04, "f at sub+0x04 == on-disk +0x2c");
XFILES_ASSERT_OFFSET(VCConversationFields, g_at_0x30, 0x08, "g at sub+0x08 == on-disk +0x30");
XFILES_ASSERT_OFFSET(VCConversationFields, h_at_0x34, 0x0c, "h at sub+0x0c == on-disk +0x34");
XFILES_ASSERT_OFFSET(VCConversationFields, i_at_0x38, 0x10, "i at sub+0x10 == on-disk +0x38");
XFILES_ASSERT_OFFSET(VCConversationFields, j_at_0x3c, 0x14, "j at sub+0x14 == on-disk +0x3c");
XFILES_ASSERT_OFFSET(VCConversationFields, k_at_0x40, 0x18, "k at sub+0x18 == on-disk +0x40");

VCConversation::VCConversation() : VCObject(), fields_() {
    std::memset(&fields_, 0, sizeof(fields_));
}

void VCConversation::Read(HDBSerializer* ser) {
    if (!ser) return;

    // 1) Base init : NPfl + vers + flag housekeeping.
    VCObject::Read(ser);
    if (ser->error()) return;

    // 2) Always-present fields : e, f, h, j (order matches on-disk read sequence).
    fields_.e_at_0x28 = ser->read_dword(0x65);  // 'e'
    fields_.f_at_0x2c = ser->read_dword(0x66);  // 'f'
    fields_.h_at_0x34 = ser->read_dword(0x68);  // 'h'
    fields_.j_at_0x3c = ser->read_dword(0x6a);  // 'j'

    // 3) Version-gated fields : g, i (added in v3).
    if (ser->current_version() > 2u) {
        fields_.g_at_0x30 = ser->read_dword(0x67);  // 'g'
        fields_.i_at_0x38 = ser->read_dword(0x69);  // 'i'
    }

    // 4) Version-gated byte field : k (added in v4).
    if (ser->current_version() > 3u) {
        fields_.k_at_0x40 = ser->read_byte_alt(0x6b);  // 'k'
    }
}

void VCConversation::Write(HDBSerializer* ser) const {
    if (!ser) return;
    VCObject::Write(ser);
    ser->write_dword(0x65, fields_.e_at_0x28);
    ser->write_dword(0x66, fields_.f_at_0x2c);
    ser->write_dword(0x68, fields_.h_at_0x34);
    ser->write_dword(0x6a, fields_.j_at_0x3c);
    if (ser->current_version() > 2u) {
        ser->write_dword(0x67, fields_.g_at_0x30);
        ser->write_dword(0x69, fields_.i_at_0x38);
    }
    if (ser->current_version() > 3u) {
        ser->write_byte(0x6b, fields_.k_at_0x40);
    }
}

}  // namespace nl
}  // namespace xfiles
