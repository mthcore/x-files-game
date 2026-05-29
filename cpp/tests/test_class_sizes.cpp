// SPDX-License-Identifier: MIT
// test_class_sizes.cpp — Verify VC* class size constants match the byte-direct layout.
//
// P1 milestone : the 51 size constants extracted from the format notes
// are compile-time available. As we implement classes in P2, replace each
// "constant only" check with `SIZE_OF_VC(VCEmail) == VC_EMAIL_SIZE` to verify
// actual struct sizeof matches.

#include "vc/_class_sizes.h"

#include <cstdio>
#include <cstdint>

using namespace xfiles::vc;

namespace {

int failures = 0;

template <typename T>
void check_equal(const char* name, T actual, T expected) {
    if (actual != expected) {
        std::printf("FAIL %s : expected 0x%x, got 0x%x\n",
                    name, static_cast<unsigned>(expected), static_cast<unsigned>(actual));
        ++failures;
    }
}

}  // namespace

int main() {
    std::printf("xfiles class size constants check\n");
    std::printf("  Total VC classes : %u\n\n", VC_CLASS_COUNT);

    // Spot-check key save classes (from W23.4 byte-direct grammar) :
    check_equal("VCEMAIL_SIZE",               VCEMAIL_SIZE,               0x78u);
    check_equal("VCEMAILREAD_SIZE",           VCEMAILREAD_SIZE,           0x78u);
    check_equal("VCEMAILPENDING_SIZE",        VCEMAILPENDING_SIZE,        0x78u);
    check_equal("VCCONVERSATIONHISTORY_SIZE", VCCONVERSATIONHISTORY_SIZE, 0x80u);
    check_equal("VCPDANOTES_SIZE",            VCPDANOTES_SIZE,            0x80u);
    check_equal("VCEVIDENCEICON_SIZE",        VCEVIDENCEICON_SIZE,        0x44u);
    check_equal("VCINVENTORY_SIZE",           VCINVENTORY_SIZE,           0x44u);
    check_equal("VCPHOTO_SIZE",               VCPHOTO_SIZE,               0x98u);
    check_equal("VCCONVERSATION_SIZE",        VCCONVERSATION_SIZE,        0x48u);
    check_equal("VCGAMESTATE_SIZE",           VCGAMESTATE_SIZE,           0x1acu);

    // Engine objects
    check_equal("VCTITLE_SIZE",               VCTITLE_SIZE,               0xa8u);
    check_equal("VCTRIGGER_SIZE",             VCTRIGGER_SIZE,             0xecu);

    // Class count
    check_equal("VC_CLASS_COUNT", VC_CLASS_COUNT, 51u);

    if (failures == 0) {
        std::printf("\nALL %u class size constants OK\n", 12u);
        return 0;
    } else {
        std::printf("\n%d FAILURES\n", failures);
        return 1;
    }
}
