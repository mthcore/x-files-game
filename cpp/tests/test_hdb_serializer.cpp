// HDBSerializer unit test (byte-direct).
//
// Exercises the typed-read contract on synthetic TLV blobs built byte
// by byte. The wire format under test is the sequential layout
// documented in include/nl/hdb_serializer.h :
//
//   [byte_tag][value_bytes]                  for read_byte/short/dword/string
//   [4-byte fourcc][value_bytes]             for read_buf_fourcc / read_dword_fourcc
//
// We check : correct value extraction, cursor advance, tag-mismatch
// error, fourcc-mismatch error, EOF mid-read, byte order (LE), short
// sign-extension. Plus a mini VCConversation-like sequence to mimic
// what a real VC* Read body will look like.

#include "nl/hdb_serializer.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

using namespace xfiles::nl;

// --- Helpers : build synthetic TLV blobs -----------------------------------

struct Blob {
    std::vector<uint8_t> bytes;

    void put_byte_tag(uint8_t tag) { bytes.push_back(tag); }
    void put_byte(uint8_t v)       { bytes.push_back(v);   }
    void put_u16_le(uint16_t v) {
        bytes.push_back(static_cast<uint8_t>(v & 0xff));
        bytes.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
    }
    void put_u32_le(uint32_t v) {
        bytes.push_back(static_cast<uint8_t>( v        & 0xff));
        bytes.push_back(static_cast<uint8_t>((v >> 8)  & 0xff));
        bytes.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
        bytes.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
    }
    void put_fourcc(uint32_t fc) {
        // ASCII order : 'NPfl' = 0x4e 0x50 0x66 0x6c
        bytes.push_back(static_cast<uint8_t>((fc >> 24) & 0xff));
        bytes.push_back(static_cast<uint8_t>((fc >> 16) & 0xff));
        bytes.push_back(static_cast<uint8_t>((fc >> 8)  & 0xff));
        bytes.push_back(static_cast<uint8_t>( fc        & 0xff));
    }
    void put_str(uint8_t tag, const std::string& s) {
        put_byte_tag(tag);
        put_u16_le(static_cast<uint16_t>(s.size()));
        for (char c : s) bytes.push_back(static_cast<uint8_t>(c));
    }
};

}  // anonymous

int main() {
    int errors = 0;
    auto fail = [&](const char* msg) {
        std::fprintf(stderr, "FAIL : %s\n", msg);
        ++errors;
    };

    // --- 1) read_byte happy path -------------------------------------------
    {
        Blob b;
        b.put_byte_tag(0x65); b.put_byte(0x42);
        InMemoryHDBSerializer ser(b.bytes.data(), b.bytes.size());
        ser.begin_record();
        uint8_t v = ser.read_byte(0x65);
        if (ser.error())   fail("1: error after read_byte");
        if (v != 0x42)     fail("1: read_byte value wrong");
        if (ser.cursor() != 2) fail("1: cursor not advanced to 2");
        if (!ser.eof())    fail("1: not at EOF");
    }

    // --- 2) read_short LE sign-extends -------------------------------------
    {
        Blob b;
        b.put_byte_tag(0x66); b.put_u16_le(0xfffe);  // -2 as int16
        InMemoryHDBSerializer ser(b.bytes.data(), b.bytes.size());
        int32_t v = ser.read_short(0x66);
        if (ser.error())   fail("2: error after read_short");
        if (v != -2)       fail("2: sign-extension wrong");
    }

    // --- 3) read_dword LE ---------------------------------------------------
    {
        Blob b;
        b.put_byte_tag(0x67); b.put_u32_le(0xdeadbeef);
        InMemoryHDBSerializer ser(b.bytes.data(), b.bytes.size());
        uint32_t v = ser.read_dword(0x67);
        if (ser.error())          fail("3: error after read_dword");
        if (v != 0xdeadbeefu)     fail("3: read_dword LE wrong");
    }

    // --- 4) read_string ----------------------------------------------------
    {
        Blob b;
        b.put_str(0x68, std::string("hello"));
        InMemoryHDBSerializer ser(b.bytes.data(), b.bytes.size());
        std::string s = ser.read_string(0x68);
        if (ser.error())   fail("4: error after read_string");
        if (s != "hello")  fail("4: read_string value wrong");
    }

    // --- 5) read_dword_fourcc 'vers' ---------------------------------------
    {
        Blob b;
        b.put_fourcc(0x76657273u);  // 'vers'
        b.put_u32_le(0x00000003u);
        InMemoryHDBSerializer ser(b.bytes.data(), b.bytes.size());
        uint32_t v = ser.read_dword_fourcc(0x76657273u);
        if (ser.error())   fail("5: error after read_dword_fourcc");
        if (v != 3u)       fail("5: read_dword_fourcc value wrong");
    }

    // --- 6) read_buf_fourcc 'NPfl' -----------------------------------------
    {
        Blob b;
        b.put_fourcc(0x4e50666cu);  // 'NPfl'
        b.put_byte(0xab); b.put_byte(0xcd);
        InMemoryHDBSerializer ser(b.bytes.data(), b.bytes.size());
        uint8_t buf[2] = {0, 0};
        ser.read_buf_fourcc(buf, 2, 0x4e50666cu);
        if (ser.error())      fail("6: error after read_buf_fourcc");
        if (buf[0] != 0xab)   fail("6: buf[0] wrong");
        if (buf[1] != 0xcd)   fail("6: buf[1] wrong");
    }

    // --- 7) Tag mismatch sets error ----------------------------------------
    {
        Blob b;
        b.put_byte_tag(0x65); b.put_u32_le(1u);
        InMemoryHDBSerializer ser(b.bytes.data(), b.bytes.size());
        (void) ser.read_dword(0x66);  // expects 'f' but stream has 'e'
        if (!ser.error())  fail("7: tag mismatch did not set error");
        // After error, subsequent reads must return 0 and stay errored.
        uint32_t v2 = ser.read_dword(0x67);
        if (v2 != 0u)      fail("7: post-error read returned non-zero");
        if (!ser.error())  fail("7: error flag cleared unexpectedly");
    }

    // --- 8) Fourcc mismatch sets error -------------------------------------
    {
        Blob b;
        b.put_fourcc(0x4e50666cu);  // 'NPfl'
        b.put_u32_le(42u);
        InMemoryHDBSerializer ser(b.bytes.data(), b.bytes.size());
        (void) ser.read_dword_fourcc(0x76657273u);  // expects 'vers'
        if (!ser.error())  fail("8: fourcc mismatch did not set error");
    }

    // --- 9) EOF mid-read sets error ----------------------------------------
    {
        Blob b;
        b.put_byte_tag(0x65); b.put_byte(0x01);  // only 1 value byte, dword wants 4
        InMemoryHDBSerializer ser(b.bytes.data(), b.bytes.size());
        (void) ser.read_dword(0x65);
        if (!ser.error())  fail("9: EOF mid-read did not set error");
    }

    // --- 10) reset() clears state for replay -------------------------------
    {
        Blob b;
        b.put_byte_tag(0x65); b.put_u32_le(0xcafeu);
        InMemoryHDBSerializer ser(b.bytes.data(), b.bytes.size());
        uint32_t a = ser.read_dword(0x65);
        ser.reset();
        uint32_t c = ser.read_dword(0x65);
        if (a != c)        fail("10: replay value differs");
        if (ser.error())   fail("10: error after reset+replay");
    }

    // --- 11) VCConversation-like mini sequence -----------------------------
    // Mimics the W23.4 grammar :
    //   +0x28 'e' uint32
    //   +0x2c 'f' uint32
    //   +0x34 'h' uint32
    //   +0x3c 'j' uint32
    //   +0x30 'g' uint32
    //   +0x38 'i' uint32
    {
        Blob b;
        b.put_byte_tag(0x65); b.put_u32_le(0x10000001u);  // e
        b.put_byte_tag(0x66); b.put_u32_le(0x10000002u);  // f
        b.put_byte_tag(0x68); b.put_u32_le(0x10000003u);  // h
        b.put_byte_tag(0x6a); b.put_u32_le(0x10000004u);  // j
        b.put_byte_tag(0x67); b.put_u32_le(0x10000005u);  // g
        b.put_byte_tag(0x69); b.put_u32_le(0x10000006u);  // i
        InMemoryHDBSerializer ser(b.bytes.data(), b.bytes.size());
        ser.begin_record();
        struct Conv { uint32_t e, f, g, h, i, j; } c = {};
        c.e = ser.read_dword(0x65);
        c.f = ser.read_dword(0x66);
        c.h = ser.read_dword(0x68);
        c.j = ser.read_dword(0x6a);
        c.g = ser.read_dword(0x67);
        c.i = ser.read_dword(0x69);
        if (ser.error())          fail("11: error during VCConv-like sequence");
        if (c.e != 0x10000001u)   fail("11: c.e wrong");
        if (c.f != 0x10000002u)   fail("11: c.f wrong");
        if (c.h != 0x10000003u)   fail("11: c.h wrong");
        if (c.j != 0x10000004u)   fail("11: c.j wrong");
        if (c.g != 0x10000005u)   fail("11: c.g wrong");
        if (c.i != 0x10000006u)   fail("11: c.i wrong");
        if (!ser.eof())           fail("11: stream not consumed");
    }

    // --- 12) Base-init-like sequence ( mimic) ------------------
    // NPfl flag buffer (2 bytes) followed by 'vers' dword.
    {
        Blob b;
        b.put_fourcc(0x4e50666cu);              // 'NPfl'
        b.put_byte(0x11); b.put_byte(0x22);     // 2-byte flag payload
        b.put_fourcc(0x76657273u);              // 'vers'
        b.put_u32_le(0x00000001u);              // version 1
        InMemoryHDBSerializer ser(b.bytes.data(), b.bytes.size());
        ser.begin_record();
        uint8_t flags[2] = {0, 0};
        ser.read_buf_fourcc(flags, 2, 0x4e50666cu);
        uint32_t vers = ser.read_dword_fourcc(0x76657273u);
        if (ser.error())     fail("12: error during base-init mimic");
        if (flags[0] != 0x11 || flags[1] != 0x22) fail("12: NPfl flags wrong");
        if (vers != 1u)      fail("12: vers wrong");
        if (!ser.eof())      fail("12: stream not consumed");
    }

    if (errors > 0) {
        std::fprintf(stderr, "FAIL : %d errors\n", errors);
        return 1;
    }
    std::printf("OK : HDBSerializer (NL.1.1) — 12 cases\n");
    return 0;
}
