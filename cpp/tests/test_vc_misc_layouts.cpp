// Validate byte-direct layouts for 3 small VC classes :
//   - VCCharacter  (44B)
//   - VCAssetCategory (48B)
//   - VCBlob       (100B)

#include "vc/vccharacter.h"
#include "vc/vcasset_category.h"
#include "vc/vcblob.h"

#include <cstddef>
#include <cstdio>

int main() {
    using namespace xfiles::vc;
    int errors = 0;

    // VCCharacter
    if (sizeof(VCCharacter) != 0x2c) ++errors;
    if (offsetof(VCCharacter, _base_scratch) != 0x04) ++errors;
    if (offsetof(VCCharacter, cache_e)       != 0x28) ++errors;
    { VCCharacter c; if (c.cache_e != 0) ++errors; }

    // VCAssetCategory
    if (sizeof(VCAssetCategory) != 0x30) ++errors;
    if (offsetof(VCAssetCategory, _base_scratch) != 0x04) ++errors;
    if (offsetof(VCAssetCategory, cache_e)       != 0x28) ++errors;
    if (offsetof(VCAssetCategory, cache_f)       != 0x2C) ++errors;
    { VCAssetCategory a; if (a.cache_e != 0 || a.cache_f != 0) ++errors; }

    // VCBlob
    if (sizeof(VCBlob) != 0x64) ++errors;
    if (offsetof(VCBlob, _base_scratch) != 0x04) ++errors;
    if (offsetof(VCBlob, sub_coll)      != 0x28) ++errors;
    if (sizeof(VCBlob().sub_coll) != 0x3C) ++errors;

    if (errors > 0) {
        std::fprintf(stderr, "FAIL : %d errors\n", errors);
        return 1;
    }
    std::printf("OK : VCCharacter 44B + VCAssetCategory 48B + VCBlob 100B byte-direct\n");
    return 0;
}
