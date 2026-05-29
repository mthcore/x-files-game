// SPDX-License-Identifier: MIT
// VCGameState — byte-exact 428-byte master save state.
//
// Byte-direct size : 0x1ac = 428 bytes
//
// Layout (byte-direct) :
//   +0x00 vtable ptr
//   +0x04..+0x27 engine internal + base data (36 bytes — TBI)
//   +0x28..+0x60 14 game-state flags (uint32 each, ctor sets various)
//     +0x4c = 1               (ctor flag, marks initialized)
//     +0x60 = 0xffffffff       (ctor flag, sentinel)
//   +0x64..+0x9f  sub-coll 't' (60B, tag 0x74)
//   +0xa0..+0xdb  sub-coll 'u' (60B, tag 0x75) — VCInventory items
//   +0xdc..+0x117 sub-coll 'v' (60B, tag 0x76) — Photo/Notes
//   +0x118..+0x153 sub-coll 'w' (60B, tag 0x77) — VCEmail family
//   +0x154..+0x18f sub-coll 'x' (60B, tag 0x78) — VCConversationHistory
//   +0x190..+0x19f tail data (16 bytes — init at +0x197 size 9)
//   +0x1a0..+0x1a8 3 uint32 tail flags (ctor : 0x68=0, 0x69=0, 0x6a=1)
//
// Reference :
//   the on-disk Read sequence — VCGameState ctor
//   notes/W21_31_GAME_MECHANISM_runtime.md — runtime byte-direct
//   notes/W22_1_FINDINGS.md — alloc count + structure
//   notes/W23_3_FINDINGS.md — snapshot mechanism

#ifndef VC_VCGAMESTATE_H
#define VC_VCGAMESTATE_H

#include "vc/vc_object.h"
#include "vc/_class_sizes.h"
#include "vc/inline_collection.h"

#include <cstdint>
#include <cstddef>
#include "vc/_size_assert.h"

namespace xfiles {
namespace vc {

class VCGameState : public VCObject {
public:
    VCGameState() noexcept;
    ~VCGameState() override;

    void vt_slot_01() override {}
    void vt_slot_02() override {}
    void vt_slot_03() override {}
    void vt_slot_04() override {}
    void vt_slot_05() override {}
    char is_leaf() const override { return 0; }
    void vt_slot_07() override {}
    void vt_slot_08() override {}
    VCObject* id_lookup(uint32_t) override { return nullptr; }
    void vt_slot_10() override {}
    uint32_t eval(uint32_t, uint32_t) override { return 0; }
    void Read(HDBContext* ctx, uint32_t param_2) override;
    void Write(HDBContext* ctx) const override;
    void vt_slot_14() override {}
    void vt_slot_15() override {}
    void vt_slot_16() override {}
    void vt_slot_17() override {}
    void vt_slot_18() override {}
    void vt_slot_19() override {}
    void vt_slot_20() override {}
    void vt_slot_21() override {}
    void vt_slot_22() override {}
    void vt_slot_23() override {}
    void vt_slot_24() override {}
    void vt_slot_25() override {}
    void vt_slot_26() override {}
    void vt_slot_27() override {}

private:
    // +0x04..+0x27 engine internal + base data (36 bytes)
    char _engine_internal[0x10];   // +0x04..+0x13
    uint32_t _npfl_flags;           // +0x14 (NPfl base flag)
    uint32_t _vers;                 // +0x18 (version dword)
    char _intermediate[0x0c];       // +0x1c..+0x27

public:
    // +0x28..+0x4b — 9 flag uint32 (ctor sets all to 0)
    uint32_t flag_28;  // +0x28
    uint32_t flag_2c;  // +0x2c
    uint32_t flag_30;  // +0x30
    uint32_t flag_34;  // +0x34
    uint32_t flag_38;  // +0x38
    uint32_t flag_3c;  // +0x3c
    uint32_t flag_40;  // +0x40
    uint32_t flag_44;  // +0x44
    uint32_t flag_48;  // +0x48

    // +0x4c ctor flag (init = 1)
    uint32_t ctor_flag_4c;

    // +0x50..+0x5c — 4 zero-flags
    uint32_t flag_50;
    uint32_t flag_54;
    uint32_t flag_58;
    uint32_t flag_5c;

    // +0x60 ctor flag (init = 0xffffffff)
    uint32_t ctor_flag_60;

    // +0x64..+0x18f : 5 sub-collections × 60 bytes each
    InlineCollection sub_t;  // +0x64  tag 't'
    InlineCollection sub_u;  // +0xa0  tag 'u' — VCInventory items
    InlineCollection sub_v;  // +0xdc  tag 'v' — Photo/Notes
    InlineCollection sub_w;  // +0x118 tag 'w' — VCEmail family
    InlineCollection sub_x;  // +0x154 tag 'x' — VCConversationHistory

    // +0x190..+0x19f : tail data (16B, init at +0x197)
    char _tail_data[0x10];

    // +0x1a0..+0x1a8 : 3 tail flags
    uint32_t tail_flag_1a0;
    uint32_t tail_flag_1a4;
    uint32_t tail_flag_1a8;
};

// P2 gate : byte-direct verification
XFILES_ASSERT_SIZE(VCGameState, VCGAMESTATE_SIZE, "VCGameState must be 0x1ac bytes");
XFILES_ASSERT_OFFSET(VCGameState, flag_28, 0x28, "flag_28 offset");
XFILES_ASSERT_OFFSET(VCGameState, ctor_flag_4c, 0x4c, "ctor_flag_4c offset");
XFILES_ASSERT_OFFSET(VCGameState, ctor_flag_60, 0x60, "ctor_flag_60 offset");
XFILES_ASSERT_OFFSET(VCGameState, sub_t, 0x64, "sub-coll 't' offset");
XFILES_ASSERT_OFFSET(VCGameState, sub_u, 0xa0, "sub-coll 'u' offset");
XFILES_ASSERT_OFFSET(VCGameState, sub_v, 0xdc, "sub-coll 'v' offset");
XFILES_ASSERT_OFFSET(VCGameState, sub_w, 0x118, "sub-coll 'w' offset");
XFILES_ASSERT_OFFSET(VCGameState, sub_x, 0x154, "sub-coll 'x' offset");
XFILES_ASSERT_OFFSET(VCGameState, tail_flag_1a8, 0x1a8, "tail offset");

}  // namespace vc
}  // namespace xfiles

#endif  // VC_VCGAMESTATE_H
