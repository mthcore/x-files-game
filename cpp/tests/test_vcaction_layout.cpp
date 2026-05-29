// Validate VCAction byte-direct layout vs the format notes.
//   +0x00 vtable
//   +0x04..+0x27 VCObject base scratch (36B)
//   +0x28..+0x63 sub_coll (56B inline map/list)
//   +0x64 kind_byte (tag 'f' 0x66)
//   +0x65..+0x67 pad
//   +0x68..+0x6B counter (zeroed in ctor)
//   sizeof = 0x6c (108B)

#include "vc/vcaction.h"

#include <cstddef>
#include <cstdio>

int main() {
    using xfiles::vc::VCAction;
    int errors = 0;

    if (sizeof(VCAction) != 0x6c) { ++errors;
        std::fprintf(stderr, "sizeof(VCAction) = %zu, expected 108\n",
                     sizeof(VCAction)); }

    if (offsetof(VCAction, _base_scratch) != 0x04) { ++errors;
        std::fprintf(stderr, "_base_scratch offset != 0x04\n"); }
    if (offsetof(VCAction, sub_coll)      != 0x28) { ++errors;
        std::fprintf(stderr, "sub_coll offset != 0x28\n"); }
    if (offsetof(VCAction, kind_byte)     != 0x64) { ++errors;
        std::fprintf(stderr, "kind_byte offset != 0x64\n"); }
    if (offsetof(VCAction, counter)       != 0x68) { ++errors;
        std::fprintf(stderr, "counter offset != 0x68\n"); }

    if (sizeof(VCAction().sub_coll) != 0x3C) { ++errors;
        std::fprintf(stderr, "sub_coll size != 56\n"); }

    VCAction a;
    if (a.kind_byte != 0) { ++errors; std::fprintf(stderr, "kind_byte not zero\n"); }
    if (a.counter   != 0) { ++errors; std::fprintf(stderr, "counter not zero\n"); }

    if (errors > 0) {
        std::fprintf(stderr, "FAIL : %d errors\n", errors);
        return 1;
    }
    std::printf("OK : VCAction 108B layout byte-direct vs vc_VCAction.md\n");
    return 0;
}
