// Validate VCNode byte-direct layout vs the format notes.

#include "vc/vcnode.h"

#include <cstddef>
#include <cstdio>

int main() {
    using xfiles::vc::VCNode;
    int errors = 0;

    if (sizeof(VCNode) != 0x70) { ++errors;
        std::fprintf(stderr, "sizeof(VCNode) = %zu, expected 112\n",
                     sizeof(VCNode)); }

    if (offsetof(VCNode, _base_scratch) != 0x04) ++errors;
    if (offsetof(VCNode, sub_object)    != 0x28) ++errors;
    if (offsetof(VCNode, payload)       != 0x40) ++errors;
    if (offsetof(VCNode, field_60)      != 0x60) ++errors;
    if (offsetof(VCNode, cache_f)       != 0x64) ++errors;
    if (offsetof(VCNode, cache_g)       != 0x68) ++errors;
    if (offsetof(VCNode, field_6c)      != 0x6C) ++errors;

    if (sizeof(VCNode().sub_object) != 0x18) ++errors;
    if (sizeof(VCNode().payload)    != 0x20) ++errors;

    VCNode n;
    if (n.field_60 != 0) ++errors;
    if (n.cache_f  != 0) ++errors;
    if (n.cache_g  != 0) ++errors;
    if (n.field_6c != 0) ++errors;

    if (errors > 0) {
        std::fprintf(stderr, "FAIL : %d errors\n", errors);
        return 1;
    }
    std::printf("OK : VCNode 112B layout byte-direct vs vc_VCNode.md\n");
    return 0;
}
