// SPDX-License-Identifier: MIT
// VCEmail family — VCEmail (0x5a), VCEmailRead (0x5b), VCEmailPending
// (0x5c), VCConversationHistory (0x56), VCPDANotes (0x58). All share
// the same Read pattern :
//   base init (NPfl + vers) + 1 polymorphic sub-object at +0x3c..+0x44
//   carrying a list/collection ; then endian-swap loop over inner items.
//
// Source : the on-disk Read sequence (VCEmail Read, 32 lines)
//          the on-disk Read sequence (VCEmailRead Read)
//          the on-disk Read sequence (VCEmailPending Read)
//          the on-disk Read sequence (VCConversationHistory Read)
//          the on-disk Read sequence (VCPDANotes Read)
//          the format notes (all are size 0x78/0x80)
//
// Our reference implementation is API-iso for the sub-object (std::unique_ptr<VCObject>)
// and skips the endian-swap post-loop (/d0 iteration over
// inner array items with vtable[+0xa4] swap call — not needed on
// little-endian hosts).

#ifndef NL_CLASSES_VC_EMAIL_FAMILY_H
#define NL_CLASSES_VC_EMAIL_FAMILY_H

#include "nl/vc_object.h"

#include <cstdint>
#include <memory>

namespace xfiles {
namespace nl {

/// Shared template-like read for the "container-of-list" pattern :
///   1) VCObject::Read (base init)
///   2) read polymorphic sub-object at the configured on-disk offset,
///      tagged 'f' (0x66)
/// Returns the sub-object via unique_ptr.
std::unique_ptr<VCObject> read_email_family_sub(HDBSerializer* ser);

/// Mirror Write.
void write_email_family_sub(HDBSerializer* ser, const VCObject* sub);

#define DECL_EMAIL_FAMILY_CLASS(name, class_id_val)                  \
    class name : public VCObject {                                    \
    public:                                                            \
        name() : VCObject(), sub_f_() {}                              \
        void Read(HDBSerializer* ser) override {                      \
            VCObject::Read(ser);                                       \
            if (!ser || ser->error()) return;                         \
            sub_f_ = read_email_family_sub(ser);                       \
        }                                                              \
        void Write(HDBSerializer* ser) const override {                \
            VCObject::Write(ser);                                       \
            write_email_family_sub(ser, sub_f_.get());                 \
        }                                                              \
        const VCObject* sub_f() const { return sub_f_.get(); }        \
        static constexpr uint32_t kClassId = class_id_val;            \
    private:                                                           \
        std::unique_ptr<VCObject> sub_f_;                              \
    }

DECL_EMAIL_FAMILY_CLASS(VCEmail,                0x5a);
DECL_EMAIL_FAMILY_CLASS(VCEmailRead,            0x5b);
DECL_EMAIL_FAMILY_CLASS(VCEmailPending,         0x5c);
DECL_EMAIL_FAMILY_CLASS(VCConversationHistory,  0x56);
DECL_EMAIL_FAMILY_CLASS(VCPDANotes,             0x58);

#undef DECL_EMAIL_FAMILY_CLASS

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASSES_VC_EMAIL_FAMILY_H
