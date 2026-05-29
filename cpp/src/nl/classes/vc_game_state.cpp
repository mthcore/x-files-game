// SPDX-License-Identifier: MIT
// VCGameState Read — decoder (master singleton).

#include "nl/classes/vc_game_state.h"
#include "nl/hdb_serializer.h"
#include "nl/vc_factory.h"

namespace xfiles {
namespace nl {

static constexpr uint32_t kFourccNPTc = 0x4e505463u;

VCGameState::VCGameState() : VCObject() {}

void VCGameState::Read(HDBSerializer* ser) {
    VCObject::Read(ser);
    if (!ser || ser->error()) return;

    // 9 dwords 'e'..'m' (tag bytes 0x65..0x6d) at +0x28..+0x48
    for (int i = 0; i < 9; ++i) {
        flags_[i] = ser->read_dword(0x65 + i);
        if (ser->error()) return;
    }

    // 'n' (0x6e) at +0x4c — uses slot 0xbc (dword_alt). Our serializer
    // doesn't have a separate byte tag for this slot ; we approximate
    // with read_dword (semantics match for valid streams).
    flag_n_ = ser->read_dword(0x6e);
    if (ser->error()) return;

    // 'o','p','q','r' (0x6f..0x72) at +0x50..+0x5c
    flag_o_ = ser->read_dword(0x6f);
    flag_p_ = ser->read_dword(0x70);
    flag_q_ = ser->read_dword(0x71);
    flag_r_ = ser->read_dword(0x72);

    // 's' (0x73) at +0x60
    flag_s_ = ser->read_dword(0x73);
    if (ser->error()) return;

    // 5 sub-objects 't'..'x' (0x74..0x78) at +0x64,+0xa0,+0xdc,+0x118,+0x154
    sub_t_ = read_obj(ser, kFourccNPTc);
    sub_u_ = read_obj(ser, kFourccNPTc);
    sub_v_ = read_obj(ser, kFourccNPTc);
    sub_w_ = read_obj(ser, kFourccNPTc);
    sub_x_ = read_obj(ser, kFourccNPTc);
    if (ser->error()) return;

    // 7 bytes 'y'..0x7f (0x79..0x7f) at +0x190..+0x196
    for (int i = 0; i < 7; ++i) {
        byte_flags_[i] = ser->read_byte(0x79 + i);
        if (ser->error()) return;
    }

    // dword 0x80 at +0x197
    dword_0x80_ = ser->read_dword(0x80);
    // byte 0x81 at +0x19f
    byte_0x81_  = ser->read_byte(0x81);
    // dword 0x82 at +0x19b (the on-disk format uses slot 0x7d alt — we use read_dword)
    dword_0x82_ = ser->read_dword(0x82);
    // dwords 0x83, 0x84 at +0x1a0, +0x1a4
    counter_0_  = ser->read_dword(0x83);
    counter_1_  = ser->read_dword(0x84);
    // short 0x85 at +0x1a8 (init = 1, read via slot 0xb0 sign-extended)
    short_value_ = ser->read_short(0x85);
}

void VCGameState::Write(HDBSerializer* ser) const {
    VCObject::Write(ser);
    for (int i = 0; i < 9; ++i) {
        ser->write_dword(static_cast<uint8_t>(0x65 + i), flags_[i]);
    }
    ser->write_dword(0x6e, flag_n_);
    ser->write_dword(0x6f, flag_o_);
    ser->write_dword(0x70, flag_p_);
    ser->write_dword(0x71, flag_q_);
    ser->write_dword(0x72, flag_r_);
    ser->write_dword(0x73, flag_s_);
    write_obj(ser, kFourccNPTc, sub_t_.get());
    write_obj(ser, kFourccNPTc, sub_u_.get());
    write_obj(ser, kFourccNPTc, sub_v_.get());
    write_obj(ser, kFourccNPTc, sub_w_.get());
    write_obj(ser, kFourccNPTc, sub_x_.get());
    for (int i = 0; i < 7; ++i) {
        ser->write_byte(static_cast<uint8_t>(0x79 + i), byte_flags_[i]);
    }
    ser->write_dword(0x80, dword_0x80_);
    ser->write_byte (0x81, byte_0x81_);
    ser->write_dword(0x82, dword_0x82_);
    ser->write_dword(0x83, counter_0_);
    ser->write_dword(0x84, counter_1_);
    ser->write_short(0x85, short_value_);
}

}  // namespace nl
}  // namespace xfiles
