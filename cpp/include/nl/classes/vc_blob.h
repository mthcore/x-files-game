// SPDX-License-Identifier: MIT
// VCBlob — class_id 0x43 (67 dec), sizeof 0x64 (100 B) byte-direct
//          but stored with a unique_ptr sub-object in our reference implementation
//          (API-iso, NOT byte-iso for the runtime struct layout).
//
// Source : the on-disk Read sequence 
//          the format notes (size 0x64)
//          the format notes (class_id 0x43)
//
// Read body (, 41 lines) :
// base init (NPfl+vers)
// sub-object at +0x28
//
// The sub-object at +0x28 is polymorphic — its concrete type is encoded
// in the HDB stream via FOURCC 'NPTc' + class_id. Our reference implementation uses
// `read_obj()` to dispatch to the right C++ class via `make_vc_object`.
//
// On-disk layout is inline storage at +0x28 (60 B of payload then padding
// to total 100 B). Our reference implementation uses `std::unique_ptr<VCObject>` for
// simplicity — sizeof differs from the on-disk layout for the on-disk layout but the read semantics match.

#ifndef NL_CLASSES_VC_BLOB_H
#define NL_CLASSES_VC_BLOB_H

#include "nl/vc_object.h"

#include <cstdint>
#include <memory>

namespace xfiles {
namespace nl {

class VCBlob : public VCObject {
public:
    VCBlob();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;

    /// Polymorphic sub-object at +0x28 in on-disk layout. Null if the
    /// stream had no sub-object or the class_id was unsupported.
    const VCObject* sub_e() const { return sub_e_.get(); }
    VCObject*       sub_e()       { return sub_e_.get(); }

    static constexpr uint32_t kClassId = 0x43;
    static constexpr std::size_t kNativeSizeOf = 0x64;

private:
    std::unique_ptr<VCObject> sub_e_;  // polymorphic, see file header
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASSES_VC_BLOB_H
