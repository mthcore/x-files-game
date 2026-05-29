// SPDX-License-Identifier: MIT
// hdb_dump — Minimal demo executable.
//
// Reads a HDB file and prints a structural summary: header, page-type
// distribution, trailer.

#include "hdb/hdb_container.h"

#include <cstdio>
#include <cstdlib>
#include <map>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: hdb_dump <path-to-XFILES.HDB>\n");
        return 1;
    }

    xfiles::hdb::HDBContainer hdb;
    if (!hdb.load_from_file(argv[1])) {
        std::fprintf(stderr, "hdb_dump: cannot open or parse %s\n", argv[1]);
        return 2;
    }

    std::printf("file       : %s\n", argv[1]);
    std::printf("size       : %zu bytes\n", hdb.raw_bytes().size());
    std::printf("pages      : %zu\n", hdb.page_count());
    std::printf("trailer    : %zu bytes\n", hdb.trailer_size());
    std::printf("\nPage tag distribution (first byte) :\n");

    std::map<uint8_t, std::size_t> dist;
    for (std::size_t i = 0; i < hdb.page_count(); ++i) {
        dist[hdb.page(i).bytes[0]]++;
    }

    static constexpr struct {
        uint8_t tag;
        const char* name;
    } kKnownTags[] = {
        {0x00, "empty (zero padding)"},
        {0xC0, "btree_freed"},
        {0xC2, "btree_internal"},
        {0xC3, "btree_leaf"},
        {0xD2, "btree_internal_alt"},
    };

    for (const auto& kt : kKnownTags) {
        const auto it = dist.find(kt.tag);
        if (it != dist.end()) {
            std::printf("  0x%02X %-22s %6zu pages\n", kt.tag, kt.name, it->second);
        }
    }
    std::size_t other = 0;
    for (const auto& [tag, count] : dist) {
        bool known = false;
        for (const auto& kt : kKnownTags) {
            if (kt.tag == tag) { known = true; break; }
        }
        if (!known) other += count;
    }
    std::printf("  ---- (other = data record pages) %6zu pages\n", other);

    return 0;
}
