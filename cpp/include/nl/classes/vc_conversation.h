// SPDX-License-Identifier: MIT
// VCConversation — byte-direct port of class 0x31 (sizeof 0x80 = 128B).
//
// Source : the per-class TLV grammar (VCConversation::Read body).
//
// TLV grammar (always 4 dwords ; +2 if vers > 2 ; +1 byte if vers > 3) :
//
//   +0x28 'e' uint32   (always)
//   +0x2c 'f' uint32   (always)
//   +0x34 'h' uint32   (always)
//   +0x3c 'j' uint32   (always)
//   +0x30 'g' uint32   (vers > 2)
//   +0x38 'i' uint32   (vers > 2)
//   +0x40 'k' byte     (vers > 3, read via slot 0x7c byte_alt)
//
// Read order in the on-disk layout matches the order above (NOT field-offset
// sorted, NOT tag-sorted). Field offsets are inside the subclass body
// the 40-byte VCObject base sits at +0x00..+0x27.

#ifndef NL_CLASSES_VC_CONVERSATION_H
#define NL_CLASSES_VC_CONVERSATION_H

#include "nl/vc_object.h"

#include <cstdint>

namespace xfiles {
namespace nl {

/// VCConversation fields living after the 40-byte VCObject base.
/// Layout matches the on-disk struct ; offsets are relative to the
/// containing class start (so +0x28 in the on-disk layout == 0 in this view).
struct VCConversationFields {
    uint32_t e_at_0x28;     // tag 'e' (always)
    uint32_t f_at_0x2c;     // tag 'f' (always)
    uint32_t g_at_0x30;     // tag 'g' (vers > 2)
    uint32_t h_at_0x34;     // tag 'h' (always)
    uint32_t i_at_0x38;     // tag 'i' (vers > 2)
    uint32_t j_at_0x3c;     // tag 'j' (always)
    uint8_t  k_at_0x40;     // tag 'k' (vers > 3, byte)
    // Trailing padding to reach class size 0x80 from base+sub = 0x28+0x18+1 = 0x41
    uint8_t  _pad[0x80 - 0x28 - 0x19];
};

class VCConversation : public VCObject {
public:
    VCConversation();

    /// Byte-direct Read decoder (VCConversation::Read).
    void Read(HDBSerializer* ser) override;

    /// Mirror Write for round-trip. Emits the same byte
    /// sequence Read consumes, conditionally on the current_version.
    void Write(HDBSerializer* ser) const override;

    const VCConversationFields& fields() const { return fields_; }
    VCConversationFields&       fields()       { return fields_; }

    static constexpr uint32_t kClassId = 0x31;

private:
    VCConversationFields fields_;
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASSES_VC_CONVERSATION_H
