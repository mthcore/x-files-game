// SPDX-License-Identifier: MIT
// VCObject base — byte-direct Read decoder.

#include "nl/vc_object.h"

#include <cstddef>
#include <cstring>
#include "vc/_size_assert.h"

namespace xfiles {
namespace nl {

// ---------------------------------------------------------------------------
// Compile-time layout contract : the 40-byte base must match the on-disk format
// binary exactly, otherwise every subclass at +0x28 would be off.
// ---------------------------------------------------------------------------
XFILES_ASSERT_SIZE(VCObjectBase, 0x28, "VCObjectBase must be exactly 40 bytes (matches xfiles)");

XFILES_ASSERT_OFFSET(VCObjectBase, vtable, 0x00, "vtable @ +0x00");
XFILES_ASSERT_OFFSET(VCObjectBase, field_0x04, 0x04, "field_0x04 @ +0x04");
XFILES_ASSERT_OFFSET(VCObjectBase, npfl_flags, 0x08, "npfl_flags @ +0x08");
XFILES_ASSERT_OFFSET(VCObjectBase, flag_byte, 0x10, "flag_byte @ +0x10");
XFILES_ASSERT_OFFSET(VCObjectBase, flag_dword, 0x14, "flag_dword @ +0x14");
XFILES_ASSERT_OFFSET(VCObjectBase, vers, 0x18, "vers @ +0x18");
XFILES_ASSERT_OFFSET(VCObjectBase, field_0x1c, 0x1c, "field_0x1c @ +0x1c");
XFILES_ASSERT_OFFSET(VCObjectBase, cache_dword, 0x20, "cache_dword @ +0x20");
XFILES_ASSERT_OFFSET(VCObjectBase, refcount, 0x24, "refcount @ +0x24");
XFILES_ASSERT_OFFSET(VCObjectBase, field_0x26, 0x26, "field_0x26 @ +0x26");

VCObject::VCObject() : base_() {
    std::memset(&base_, 0, sizeof(base_));
}

// ---------------------------------------------------------------------------
// VCObject::Read = byte-direct Read decoder
//
// On-disk read sequence (abbreviated) :
//
//   iVar2 = *param_1;                                  // serializer vtable
//   (**(code **)(iVar2 + 0xa8))();                     // begin_record
//   (**(code **)(iVar2 + 0x74))(this+8, 2, 'NPfl');    // read 2B NPfl
//   if (meta->byte_at_0xf) {                            // has_version_field
//       this->vers = (**(code **)(iVar2+0xbc))('vers');
//   this->flag_byte  = (this->flag_byte  & 0xfc) | 0x60;
//   this->flag_dword =  this->flag_dword & 0xfffffff0;
//   this->refcount   = 1;
// ---------------------------------------------------------------------------
void VCObject::Read(HDBSerializer* ser) {
    if (!ser) return;

    // Slot 0xa8 : open the record (no-op in InMemoryHDBSerializer).
    ser->begin_record();

    // Slot 0x74 : read 2 bytes guarded by 'NPfl' fourcc into +0x08.
    ser->read_buf_fourcc(&base_.npfl_flags, 2, k_fourcc_npfl);

    // Slot 0xbc : conditional vers read (per-class metadata).
    if (ser->has_version_field()) {
        base_.vers = ser->read_dword_fourcc(k_fourcc_vers);
        // Propagate the just-read version to the serializer so the
        // subclass's Read sees the correct value via current_version().
        // On disk, the format stores vers in a per-record context (param_1[3]+4)
        // that subclass Reads dereference for version-gated fields.
        // read_obj() saves/restores this around nested reads.
        if (auto* mem = dynamic_cast<InMemoryHDBSerializer*>(ser)) {
            mem->set_current_version(base_.vers);
        }
    }

    // Init/clean flag bits (matches the on-disk bit twiddling after vers read).
    base_.flag_byte  = static_cast<uint8_t>((base_.flag_byte & 0xfcu) | 0x60u);
    base_.flag_dword = base_.flag_dword & 0xfffffff0u;
    base_.refcount   = 1;
}

// ---------------------------------------------------------------------------
// VCObject::Write — emits NPfl (2 bytes) + vers (4 bytes) matching what
// Read consumes. Subclasses chain to this first then write their own
// fields in the same order Read consumed them.
// ---------------------------------------------------------------------------
void VCObject::Write(HDBSerializer* ser) const {
    if (!ser) return;
    ser->begin_write_record();
    ser->write_buf_fourcc(&base_.npfl_flags, 2, k_fourcc_npfl);
    if (ser->has_version_field()) {
        ser->write_dword_fourcc(k_fourcc_vers, base_.vers);
    }
}

}  // namespace nl
}  // namespace xfiles
