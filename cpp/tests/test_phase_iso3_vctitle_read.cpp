// Step 3 test: VCTitle::Read byte-direct port.
//
// 1. Synthetic TLV blob with all 4 strings + 4 numerics → verify each
//    field round-trips correctly.
// 2. Partial blob (missing some tags) → verify miss tolerance.

#include "vc/vc_title_reader.h"
#include "hdb/tlv_reader.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

std::vector<uint8_t> u16_be(uint16_t v) { return {uint8_t(v>>8), uint8_t(v)}; }
std::vector<uint8_t> u32_be(uint32_t v) {
    return {uint8_t(v>>24), uint8_t(v>>16), uint8_t(v>>8), uint8_t(v)};
}

std::vector<uint8_t> tag_u32(uint8_t tag, uint32_t v) {
    std::vector<uint8_t> b; b.push_back(tag);
    auto l = u16_be(4); b.insert(b.end(), l.begin(), l.end());
    auto p = u32_be(v); b.insert(b.end(), p.begin(), p.end());
    return b;
}
std::vector<uint8_t> tag_u16(uint8_t tag, uint16_t v) {
    std::vector<uint8_t> b; b.push_back(tag);
    auto l = u16_be(2); b.insert(b.end(), l.begin(), l.end());
    auto p = u16_be(v); b.insert(b.end(), p.begin(), p.end());
    return b;
}
std::vector<uint8_t> tag_str(uint8_t tag, const char* s) {
    std::vector<uint8_t> b; b.push_back(tag);
    auto n = static_cast<uint16_t>(std::strlen(s) + 1);
    auto l = u16_be(n); b.insert(b.end(), l.begin(), l.end());
    b.insert(b.end(), s, s + std::strlen(s));
    b.push_back(0);
    return b;
}

std::vector<uint8_t> concat(std::initializer_list<std::vector<uint8_t>> parts) {
    std::vector<uint8_t> out;
    for (const auto& p : parts) out.insert(out.end(), p.begin(), p.end());
    return out;
}

}  // anon

int main() {
    using namespace xfiles;
    int errors = 0;
    auto fail = [&](const char* msg) {
        std::fprintf(stderr, "FAIL : %s\n", msg);
        ++errors;
    };

    // --- 1) Full payload : all 8 fields ---
    {
        auto blob = concat({
            tag_str('f', "MainTitle"),
            tag_str('g', "Subtitle"),
            tag_str('h', "Author"),
            tag_str('i', "Build"),
            tag_u32('j', 0xCAFEBABE),
            tag_u32('k', 42),
            tag_u16('l', 0x1234),
            tag_u16('m', 0xABCD),
        });
        hdb::MemoryTLVReader r(blob.data(), blob.size());
        vc::VCTitleData td;
        bool ok = vc::read_vc_title(r, &td);
        if (!ok) fail("read_vc_title returned false on full blob");
        if (td.string_f != "MainTitle") {
            std::fprintf(stderr, "  string_f = '%s'\n", td.string_f.c_str());
            fail("string_f != MainTitle");
        }
        if (td.string_g != "Subtitle") fail("string_g != Subtitle");
        if (td.string_h != "Author")   fail("string_h != Author");
        if (td.string_i != "Build")    fail("string_i != Build");
        if (td.field_9c != 0xCAFEBABEu) fail("field_9c != 0xCAFEBABE");
        if (td.field_a0 != 42u)         fail("field_a0 != 42");
        if (td.field_a4 != 0x1234u)     fail("field_a4 != 0x1234");
        if (td.field_a6 != 0xABCDu)     fail("field_a6 != 0xABCD");
    }

    // --- 2) Partial payload : only string_f + field_9c, rest missing ---
    {
        auto blob = concat({
            tag_str('f', "OnlyOne"),
            tag_u32('j', 99),
        });
        hdb::MemoryTLVReader r(blob.data(), blob.size());
        vc::VCTitleData td;
        bool ok = vc::read_vc_title(r, &td);
        if (!ok) fail("read_vc_title returned false on partial blob");
        if (td.string_f != "OnlyOne") fail("partial : string_f != OnlyOne");
        if (!td.string_g.empty())     fail("partial : string_g not empty");
        if (td.field_9c != 99u)       fail("partial : field_9c != 99");
        if (td.field_a0 != 0u)        fail("partial : field_a0 not 0");
    }

    // --- 3) Empty blob ---
    {
        hdb::MemoryTLVReader r(nullptr, 0);
        vc::VCTitleData td;
        bool ok = vc::read_vc_title(r, &td);
        if (ok) fail("read_vc_title returned true on empty blob");
    }

    if (errors > 0) {
        std::fprintf(stderr, "FAIL : %d errors\n", errors);
        return 1;
    }
    std::printf("OK : Step 3 — VCTitle::Read (4 strings + 2 u32 + 2 u16)\n");
    return 0;
}
