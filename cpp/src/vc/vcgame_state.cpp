// SPDX-License-Identifier: MIT
// VCGameState.cpp — byte-direct ctor + Read/Write per the on-disk + 

#include "vc/vcgame_state.h"
#include "hdb/hdb_context.h"
#include "hdb/hdb_tlv.h"

#include <cstring>

namespace xfiles {
namespace vc {

VCGameState::VCGameState() noexcept
    : _npfl_flags(0), _vers(0),
      flag_28(0), flag_2c(0), flag_30(0), flag_34(0),
      flag_38(0), flag_3c(0), flag_40(0), flag_44(0), flag_48(0),
      ctor_flag_4c(1),
      flag_50(0), flag_54(0), flag_58(0), flag_5c(0),
      ctor_flag_60(0xffffffffu),
      tail_flag_1a0(0), tail_flag_1a4(0), tail_flag_1a8(1) {
    std::memset(_engine_internal, 0, sizeof(_engine_internal));
    std::memset(_intermediate, 0, sizeof(_intermediate));
    std::memset(_tail_data, 0, sizeof(_tail_data));
    // 5 sub-colls have parent_ptr set to this
    sub_t.parent_ptr = this;
    sub_u.parent_ptr = this;
    sub_v.parent_ptr = this;
    sub_w.parent_ptr = this;
    sub_x.parent_ptr = this;
}

VCGameState::~VCGameState() = default;

void VCGameState::Read(HDBContext* ctx, uint32_t /*param_2*/) {
    if (!ctx) return;
    // Per (the on-disk Read sequence) :
    //   base init + 14 uint32 fields tags 'e'..'s' + 5 sub-coll reads tags 't','u','v','w','x'.
    ctx->read_base_init(_npfl_flags, _vers);
    auto& r = ctx->reader();
    flag_28      = r.read_uint32_field(0x65);  // 'e'
    flag_2c      = r.read_uint32_field(0x66);  // 'f'
    flag_30      = r.read_uint32_field(0x67);  // 'g'
    flag_34      = r.read_uint32_field(0x68);  // 'h'
    flag_38      = r.read_uint32_field(0x69);  // 'i'
    flag_3c      = r.read_uint32_field(0x6a);  // 'j'
    flag_40      = r.read_uint32_field(0x6b);  // 'k'
    flag_44      = r.read_uint32_field(0x6c);  // 'l'
    flag_48      = r.read_uint32_field(0x6d);  // 'm'
    ctor_flag_4c = r.read_uint32_field(0x6e);  // 'n'
    flag_50      = r.read_uint32_field(0x6f);  // 'o'
    flag_54      = r.read_uint32_field(0x70);  // 'p'
    flag_58      = r.read_uint32_field(0x71);  // 'q'
    flag_5c      = r.read_uint32_field(0x72);  // 'r'
    ctor_flag_60 = r.read_uint32_field(0x73);  // 's'
    // 5 sub-coll reads (tags 't','u','v','w','x') — in the on-disk format
    // P5 polish : will invoke InlineCollection::Read for each.
    // For now, sub-colls remain at default (head_id=3, items_id=0 = fresh).
}

void VCGameState::Write(HDBContext* ctx) const {
    if (!ctx) return;
    ctx->write_base_init(_npfl_flags, _vers);
    auto& w = ctx->writer();
    w.write_uint32_field(0x65, flag_28);
    w.write_uint32_field(0x66, flag_2c);
    w.write_uint32_field(0x67, flag_30);
    w.write_uint32_field(0x68, flag_34);
    w.write_uint32_field(0x69, flag_38);
    w.write_uint32_field(0x6a, flag_3c);
    w.write_uint32_field(0x6b, flag_40);
    w.write_uint32_field(0x6c, flag_44);
    w.write_uint32_field(0x6d, flag_48);
    w.write_uint32_field(0x6e, ctor_flag_4c);
    w.write_uint32_field(0x6f, flag_50);
    w.write_uint32_field(0x70, flag_54);
    w.write_uint32_field(0x71, flag_58);
    w.write_uint32_field(0x72, flag_5c);
    w.write_uint32_field(0x73, ctor_flag_60);
    // Sub-colls Write : P5 polish.
}

}  // namespace vc
}  // namespace xfiles
