// Step 1 test: MemoryTLVReader byte-direct.
//
// Wire format under test :
//   byte 0     : tag (ASCII)
//   bytes 1-2  : big-endian uint16 length
//   bytes ...  : payload (length bytes)

#include "hdb/tlv_reader.h"

#include <cstdio>
#include <cstdint>
#include <vector>

namespace {

// Helper to build a TLV blob in-test.
std::vector<uint8_t> build_blob(std::initializer_list<std::vector<uint8_t>> entries) {
    std::vector<uint8_t> out;
    for (const auto& e : entries) {
        out.insert(out.end(), e.begin(), e.end());
    }
    return out;
}

std::vector<uint8_t> u32_be(uint32_t v) {
    return { uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v) };
}
std::vector<uint8_t> u16_be(uint16_t v) {
    return { uint8_t(v >> 8), uint8_t(v) };
}
std::vector<uint8_t> tag_u32(uint8_t tag, uint32_t v) {
    std::vector<uint8_t> out;
    out.push_back(tag);
    auto len = u16_be(4); out.insert(out.end(), len.begin(), len.end());
    auto pl  = u32_be(v); out.insert(out.end(), pl.begin(), pl.end());
    return out;
}
std::vector<uint8_t> tag_u16(uint8_t tag, uint16_t v) {
    std::vector<uint8_t> out;
    out.push_back(tag);
    auto len = u16_be(2); out.insert(out.end(), len.begin(), len.end());
    auto pl  = u16_be(v); out.insert(out.end(), pl.begin(), pl.end());
    return out;
}
std::vector<uint8_t> tag_str(uint8_t tag, const char* s) {
    std::vector<uint8_t> out;
    out.push_back(tag);
    auto n = static_cast<uint16_t>(std::strlen(s) + 1);  // +1 NUL
    auto len = u16_be(n); out.insert(out.end(), len.begin(), len.end());
    out.insert(out.end(), s, s + std::strlen(s));
    out.push_back(0);  // NUL terminator (xfiles strings)
    return out;
}

}  // anon

int main() {
    using namespace xfiles::hdb;
    int errors = 0;
    auto fail = [&](const char* msg) {
        std::fprintf(stderr, "FAIL : %s\n", msg);
        ++errors;
    };

    // ---- Test 1 : read uint32 by tag ----
    {
        auto blob = build_blob({
            tag_u32('e', 0xDEADBEEF),
            tag_u32('f', 42),
        });
        MemoryTLVReader r(blob.data(), blob.size());
        if (r.read_uint32('e') != 0xDEADBEEFu) fail("read_uint32('e')");
        if (r.read_uint32('f') != 42u)         fail("read_uint32('f')");
        if (r.any_miss())                       fail("unexpected miss after sequential reads");
    }

    // ---- Test 2 : read uint16 by tag ----
    {
        auto blob = build_blob({
            tag_u16('l', 0xABCD),
            tag_u16('m', 0x1234),
        });
        MemoryTLVReader r(blob.data(), blob.size());
        if (r.read_uint16('l') != 0xABCDu) fail("read_uint16('l')");
        if (r.read_uint16('m') != 0x1234u) fail("read_uint16('m')");
    }

    // ---- Test 3 : read_string ----
    {
        auto blob = build_blob({
            tag_str('n', "MainMenu"),
            tag_str('o', "Quit"),
        });
        MemoryTLVReader r(blob.data(), blob.size());
        std::string a = r.read_string('n');
        if (a != "MainMenu") {
            std::fprintf(stderr, "  read_string('n') = '%s'\n", a.c_str());
            fail("read_string('n') != MainMenu");
        }
        std::string b = r.read_string('o');
        if (b != "Quit") fail("read_string('o') != Quit");
    }

    // ---- Test 4 : miss returns 0 / empty + sets any_miss() ----
    {
        auto blob = build_blob({ tag_u32('e', 99) });
        MemoryTLVReader r(blob.data(), blob.size());
        if (r.read_uint32('z') != 0u)       fail("read_uint32('z') (missing) != 0");
        if (!r.any_miss())                   fail("any_miss not set after miss");
    }

    // ---- Test 5 : read_uint32_fourcc 'vers' ----
    {
        // Layout : 'NPfl' fourcc, then 'vers' fourcc + uint32 value.
        std::vector<uint8_t> blob;
        auto npfl = u32_be(kFourcc_NPfl); blob.insert(blob.end(), npfl.begin(), npfl.end());
        auto vers = u32_be(kFourcc_vers); blob.insert(blob.end(), vers.begin(), vers.end());
        auto v    = u32_be(7);            blob.insert(blob.end(), v.begin(),    v.end());
        MemoryTLVReader r(blob.data(), blob.size());
        r.prelude();
        if (r.read_uint32_fourcc(kFourcc_vers) != 7u) fail("vers != 7");
    }

    if (errors > 0) {
        std::fprintf(stderr, "FAIL : %d errors\n", errors);
        return 1;
    }
    std::printf("OK : Step 1 — MemoryTLVReader (uint32/uint16/string/miss/fourcc)\n");
    return 0;
}
