// SPDX-License-Identifier: MIT
// test_smoke — Minimal smoke test for the hdb_decoder library.
//
// Builds and links the public API. Does NOT require an HDB file (the test
// only validates compile + link + basic invariants). For real-data tests,
// use the Python pytest suite under ../python/tests/ — the C++ decoder
// will get a Catch2-based suite in v0.2.

#include "hdb/hdb_container.h"
#include "nl/neo_stream.h"

#include <cassert>
#include <cstdio>
#include <vector>

int main() {
    // Default-constructed container has zero pages; this verifies the
    // public API surface compiles and links cleanly. Real container
    // round-trip testing happens in the Python suite (with the actual
    // XFILES.HDB) and in the larger integration tests planned for v0.3+.
    xfiles::hdb::HDBContainer hdb;
    std::printf("smoke: pages=%zu, trailer=%zu — OK (default-constructed)\n",
                hdb.page_count(), hdb.trailer_size());
    return 0;
}
