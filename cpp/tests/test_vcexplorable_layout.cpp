// Validate VCExplorable byte-direct layout vs the format notes.

#include "vc/vcexplorable.h"

#include <cstddef>
#include <cstdio>

int main() {
    using xfiles::vc::VCExplorable;
    int errors = 0;

    if (sizeof(VCExplorable) != 0x4c) { ++errors;
        std::fprintf(stderr, "sizeof(VCExplorable) = %zu, expected 76\n",
                     sizeof(VCExplorable)); }

    if (offsetof(VCExplorable, _base_scratch)   != 0x04) ++errors;
    if (offsetof(VCExplorable, sub_object_ptr)  != 0x28) ++errors;
    if (offsetof(VCExplorable, sub_fields)      != 0x2C) ++errors;
    if (offsetof(VCExplorable, heap_owned)      != 0x38) ++errors;
    if (offsetof(VCExplorable, more_fields)     != 0x3C) ++errors;
    if (offsetof(VCExplorable, cache_f)         != 0x44) ++errors;
    if (offsetof(VCExplorable, field_48)        != 0x48) ++errors;

    VCExplorable e;
    if (e.sub_object_ptr != 0) ++errors;
    if (e.heap_owned     != 0) ++errors;
    if (e.cache_f        != 0) ++errors;
    if (e.field_48       != 0) ++errors;

    if (errors > 0) {
        std::fprintf(stderr, "FAIL : %d errors\n", errors);
        return 1;
    }
    std::printf("OK : VCExplorable 76B layout byte-direct vs vc_VCExplorable.md\n");
    return 0;
}
