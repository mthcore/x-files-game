// Validate byte-direct layout for the 10 List<T> classes (vc_family_list.md).
// All share : sizeof=100B, sub_coll at +0x28..+0x63 (60B), base scratch +0x04..+0x27.

#include "vc/vcchar_view_list.h"
#include "vc/vcconversation_list.h"
#include "vc/vcnav__list.h"
#include "vc/vcasset_ref_list.h"
#include "vc/vcexplorable_list.h"
#include "vc/vchot_spot_list.h"
#include "vc/vcaction_list.h"
#include "vc/vcidea_map_list.h"
#include "vc/vctrigger_list.h"
#include "vc/vcstd_action_list.h"

#include <cstddef>
#include <cstdio>

#define CHECK_LIST(T)                                                          \
    do {                                                                        \
        if (sizeof(xfiles::vc::T) != 0x64) { ++errors;                          \
            std::fprintf(stderr, #T " size != 100\n"); }                        \
        if (offsetof(xfiles::vc::T, _base_scratch) != 0x04) { ++errors;         \
            std::fprintf(stderr, #T " _base_scratch off\n"); }                  \
        if (offsetof(xfiles::vc::T, sub_coll)      != 0x28) { ++errors;         \
            std::fprintf(stderr, #T " sub_coll off\n"); }                       \
        if (sizeof(xfiles::vc::T().sub_coll) != 0x3C) { ++errors;               \
            std::fprintf(stderr, #T " sub_coll size\n"); }                      \
    } while (0)

int main() {
    int errors = 0;
    CHECK_LIST(VCCharViewList);
    CHECK_LIST(VCConversationList);
    CHECK_LIST(VCNavList);
    CHECK_LIST(VCAssetRefList);
    CHECK_LIST(VCExplorableList);
    CHECK_LIST(VCHotSpotList);
    CHECK_LIST(VCActionList);
    CHECK_LIST(VCIdeaMapList);
    CHECK_LIST(VCTriggerList);
    CHECK_LIST(VCStdActionList);

    if (errors > 0) {
        std::fprintf(stderr, "FAIL : %d errors\n", errors);
        return 1;
    }
    std::printf("OK : 10 List<T> classes 100B byte-direct (vc_family_list)\n");
    return 0;
}
