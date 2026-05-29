// Validate VCCursor byte-direct layout vs the format notes.
//   +0x00 vtable
//   +0x04..+0x27 VCObject base scratch (36B)
//   +0x28..+0x33 sprite_string (CString sub-object 12B)
//   +0x34 cache_f (tag 'f' 0x66)
//   +0x38 cache_g (tag 'g' 0x67)
//   sizeof = 0x3c (60B)

#include "vc/vccursor.h"

#include <cstddef>
#include <cstdio>

int main() {
    using xfiles::vc::VCCursor;
    int errors = 0;

    if (sizeof(VCCursor) != 0x3c) { ++errors;
        std::fprintf(stderr, "sizeof(VCCursor) = %zu, expected 60\n",
                     sizeof(VCCursor)); }

    if (offsetof(VCCursor, _base_scratch) != 0x04) { ++errors;
        std::fprintf(stderr, "_base_scratch offset != 0x04\n"); }
    if (offsetof(VCCursor, sprite_string) != 0x28) { ++errors;
        std::fprintf(stderr, "sprite_string offset != 0x28\n"); }
    if (offsetof(VCCursor, cache_f)       != 0x34) { ++errors;
        std::fprintf(stderr, "cache_f offset != 0x34\n"); }
    if (offsetof(VCCursor, cache_g)       != 0x38) { ++errors;
        std::fprintf(stderr, "cache_g offset != 0x38\n"); }

    if (sizeof(VCCursor().sprite_string) != 0x0C) { ++errors;
        std::fprintf(stderr, "sprite_string size != 12\n"); }

    // Ctor zeroes everything.
    VCCursor c;
    if (c.cache_f != 0) { ++errors; std::fprintf(stderr, "cache_f not zero\n"); }
    if (c.cache_g != 0) { ++errors; std::fprintf(stderr, "cache_g not zero\n"); }

    if (errors > 0) {
        std::fprintf(stderr, "FAIL : %d errors\n", errors);
        return 1;
    }
    std::printf("OK : VCCursor 60B layout byte-direct vs vc_VCCursor.md\n");
    return 0;
}
