// Validate VCTrigger byte-direct layout vs the format notes.
// VCTrigger is 236 bytes (0xec) with this exact field map :
//   +0x00 vtable
//   +0x04..+0x27 VCObject base scratch
//   +0x28..+0x5B sub-coll A (52B = capacity 8)  — Conditions
//   +0x5C..+0x9F sub-coll B (68B = capacity 12) — Actions
//   +0xA0..+0xE3 sub-coll C (68B = capacity 4)  — Targets
//   +0xE4..+0xE7 cache_e (dword, tag 'e' 0x65)
//   +0xE8        cache_f (byte,  tag 'f' 0x66)
//   +0xE9..+0xEB pad

#include "vc/vctrigger.h"

#include <cstddef>
#include <cstdio>

int main() {
    using xfiles::vc::VCTrigger;
    int errors = 0;

    if (sizeof(VCTrigger) != 0xec) { ++errors;
        std::fprintf(stderr, "sizeof(VCTrigger) = %zu, expected 236\n",
                     sizeof(VCTrigger)); }

    if (offsetof(VCTrigger, _base_scratch)      != 0x04) { ++errors;
        std::fprintf(stderr, "_base_scratch offset != 0x04\n"); }
    if (offsetof(VCTrigger, subcoll_conditions) != 0x28) { ++errors;
        std::fprintf(stderr, "subcoll_conditions offset != 0x28\n"); }
    if (offsetof(VCTrigger, subcoll_actions)    != 0x5C) { ++errors;
        std::fprintf(stderr, "subcoll_actions offset != 0x5C\n"); }
    if (offsetof(VCTrigger, subcoll_targets)    != 0xA0) { ++errors;
        std::fprintf(stderr, "subcoll_targets offset != 0xA0\n"); }
    if (offsetof(VCTrigger, cache_e)            != 0xE4) { ++errors;
        std::fprintf(stderr, "cache_e offset != 0xE4\n"); }
    if (offsetof(VCTrigger, cache_f)            != 0xE8) { ++errors;
        std::fprintf(stderr, "cache_f offset != 0xE8\n"); }

    // Sub-coll sizes per W5 doc :
    //   A (Conditions, capacity 8)  : 0x5C - 0x28 = 0x34 = 52B
    //   B (Actions,    capacity 12) : 0xA0 - 0x5C = 0x44 = 68B
    //   C (Targets,    capacity 4)  : 0xE4 - 0xA0 = 0x44 = 68B
    if (sizeof(VCTrigger().subcoll_conditions) != 0x34) { ++errors;
        std::fprintf(stderr, "subcoll_conditions size != 52\n"); }
    if (sizeof(VCTrigger().subcoll_actions)    != 0x44) { ++errors;
        std::fprintf(stderr, "subcoll_actions size != 68\n"); }
    if (sizeof(VCTrigger().subcoll_targets)    != 0x44) { ++errors;
        std::fprintf(stderr, "subcoll_targets size != 68\n"); }

    // Ctor zeroes everything.
    VCTrigger t;
    if (t.cache_e != 0) { ++errors; std::fprintf(stderr, "cache_e not zero\n"); }
    if (t.cache_f != 0) { ++errors; std::fprintf(stderr, "cache_f not zero\n"); }

    if (errors > 0) {
        std::fprintf(stderr, "FAIL : %d errors\n", errors);
        return 1;
    }
    std::printf("OK : VCTrigger 236B layout byte-direct vs vc_VCTrigger.md\n");
    return 0;
}
