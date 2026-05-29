// SPDX-License-Identifier: MIT
// VCGameState — master singleton (class_id 0x57, sizeof 0x1ac = 428 B).
//
// Source : NeoPersist 3.0 on-disk format
//          the on-disk Read sequence 
//
// Layout : 31 tags total — 9 dwords (e..m) + 1 dword_alt 'n' + 4 dwords +
// 1 dword 's' + 5 polymorphic sub-objects (t,u,v,w,x) + 7 bytes (y..0x7f)
// + 1 dword 0x80 + 1 byte 0x81 + 1 dword 0x82 + 2 dwords (0x83,0x84) +
// 1 short 0x85.
//
// In our API-iso reference implementation, the 5 sub-objects are stored as unique_ptr<VCObject>
// (inline storage on disk at +0x64/+0xa0/+0xdc/+0x118/+0x154 with
// a shared vtable).

#ifndef NL_CLASSES_VC_GAME_STATE_H
#define NL_CLASSES_VC_GAME_STATE_H

#include "nl/vc_object.h"

#include <cstdint>
#include <memory>

namespace xfiles {
namespace nl {

class VCGameState : public VCObject {
public:
    VCGameState();
    void Read(HDBSerializer* ser) override;
    void Write(HDBSerializer* ser) const override;

    // 9 game_flags dwords 'e'..'m' at +0x28..+0x48
    uint32_t game_flag(int idx) const {
        return (idx >= 0 && idx < 9) ? flags_[idx] : 0;
    }
    // Special dword_alt 'n' at +0x4c (init = 1, read via slot 0xbc)
    uint32_t flag_n() const { return flag_n_; }
    // dwords 'o','p','q','r' at +0x50..+0x5c
    uint32_t flag_o() const { return flag_o_; }
    uint32_t flag_p() const { return flag_p_; }
    uint32_t flag_q() const { return flag_q_; }
    uint32_t flag_r() const { return flag_r_; }
    // 's' at +0x60 (init = 0xffffffff sentinel)
    uint32_t flag_s() const { return flag_s_; }

    // 5 polymorphic sub-objects (collections inline natively)
    const VCObject* sub_t() const { return sub_t_.get(); }  // evidence_list ?
    const VCObject* sub_u() const { return sub_u_.get(); }  // email_pending_list ?
    const VCObject* sub_v() const { return sub_v_.get(); }  // email_read_list ?
    const VCObject* sub_w() const { return sub_w_.get(); }  // conversation_history ?
    const VCObject* sub_x() const { return sub_x_.get(); }  // triggers/explorable ?

    // 7 bytes 'y'..0x7f at +0x190..+0x196
    uint8_t byte_flag(int idx) const {
        return (idx >= 0 && idx < 7) ? byte_flags_[idx] : 0;
    }
    // dword 0x80 at +0x197
    uint32_t dword_0x80() const { return dword_0x80_; }
    // byte 0x81 at +0x19f
    uint8_t  byte_0x81() const { return byte_0x81_; }
    // dword 0x82 at +0x19b
    uint32_t dword_0x82() const { return dword_0x82_; }
    // dwords 0x83, 0x84 at +0x1a0, +0x1a4
    uint32_t counter_0() const { return counter_0_; }
    uint32_t counter_1() const { return counter_1_; }
    // short 0x85 at +0x1a8 (read as int32, init = 1)
    int32_t  short_value() const { return short_value_; }

    static constexpr uint32_t kClassId = 0x57;
    static constexpr std::size_t kNativeSizeOf = 0x1ac;

private:
    uint32_t flags_[9] = {};        // e..m
    uint32_t flag_n_ = 0;
    uint32_t flag_o_ = 0, flag_p_ = 0, flag_q_ = 0, flag_r_ = 0;
    uint32_t flag_s_ = 0;
    std::unique_ptr<VCObject> sub_t_, sub_u_, sub_v_, sub_w_, sub_x_;
    uint8_t  byte_flags_[7] = {};   // y..0x7f
    uint32_t dword_0x80_ = 0;
    uint8_t  byte_0x81_ = 0;
    uint32_t dword_0x82_ = 0;
    uint32_t counter_0_ = 0, counter_1_ = 0;
    int32_t  short_value_ = 0;
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_CLASSES_VC_GAME_STATE_H
