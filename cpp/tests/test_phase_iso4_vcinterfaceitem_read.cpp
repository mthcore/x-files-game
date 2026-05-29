// Step 4 test: VCInterfaceItem::Read byte-direct.

#include "vc/vc_interface_item_reader.h"
#include "hdb/tlv_reader.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

std::vector<uint8_t> u16_be(uint16_t v) { return {uint8_t(v>>8), uint8_t(v)}; }
std::vector<uint8_t> u32_be(uint32_t v) {
    return {uint8_t(v>>24), uint8_t(v>>16), uint8_t(v>>8), uint8_t(v)};
}
int16_t hi16(int v) { return static_cast<int16_t>(v); }

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

// Build a "bbox" sub-object payload : 8 bytes of (x, y, w, h) int16 BE.
std::vector<uint8_t> tag_bbox(uint8_t tag, int x, int y, int w, int h) {
    std::vector<uint8_t> b; b.push_back(tag);
    // payload : 4 int16 BE
    std::vector<uint8_t> pl;
    auto put = [&](int16_t v) { pl.push_back(uint8_t(uint16_t(v) >> 8)); pl.push_back(uint8_t(v)); };
    put(static_cast<int16_t>(x));
    put(static_cast<int16_t>(y));
    put(static_cast<int16_t>(w));
    put(static_cast<int16_t>(h));
    auto l = u16_be(static_cast<uint16_t>(pl.size()));
    b.insert(b.end(), l.begin(), l.end());
    b.insert(b.end(), pl.begin(), pl.end());
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

    // --- 1) Full payload : bbox + 2 u32 + 1 u16 + 1 i16 ---
    {
        auto blob = concat({
            tag_bbox('e', 100, 200, 80, 30),
            tag_u32 ('f', 0xCAFE),
            tag_u32 ('g', 0x1234),
            tag_u16 ('h', 0xABCD),
            tag_u16 ('i', static_cast<uint16_t>(int16_t(-5))),
        });
        hdb::MemoryTLVReader r(blob.data(), blob.size());
        vc::VCInterfaceItemData it;
        bool ok = vc::read_vc_interface_item(r, &it);
        if (!ok) fail("read_vc_interface_item returned false on full blob");

        if (!it.bbox.ok) fail("bbox not decoded ok");
        if (it.bbox.x != 100) fail("bbox.x != 100");
        if (it.bbox.y != 200) fail("bbox.y != 200");
        if (it.bbox.w != 80)  fail("bbox.w != 80");
        if (it.bbox.h != 30)  fail("bbox.h != 30");

        if (it.field_44 != 0xCAFEu) fail("field_44 != 0xCAFE");
        if (it.field_48 != 0x1234u) fail("field_48 != 0x1234");
        if (it.field_4c != 0xABCDu) fail("field_4c != 0xABCD");
        if (it.field_50 != -5)      fail("field_50 != -5");
    }

    // --- 2) No bbox / missing tags ---
    {
        auto blob = concat({
            tag_u32('f', 99),
        });
        hdb::MemoryTLVReader r(blob.data(), blob.size());
        vc::VCInterfaceItemData it;
        bool ok = vc::read_vc_interface_item(r, &it);
        if (!ok) fail("read_vc_interface_item returned false on partial");
        if (it.bbox.ok) fail("bbox shouldn't be ok with no 'e' tag");
        if (it.field_44 != 99u) fail("field_44 != 99");
    }

    if (errors > 0) {
        std::fprintf(stderr, "FAIL : %d errors\n", errors);
        return 1;
    }
    std::printf("OK : Step 4 — VCInterfaceItem::Read "
                "(bbox + tags f/g/h/i)\n");
    return 0;
}
