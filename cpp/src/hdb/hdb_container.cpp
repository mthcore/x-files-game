// SPDX-License-Identifier: MIT
// HDB container implementation.

#include "hdb/hdb_container.h"

#include <cstdio>
#include <cstring>

namespace xfiles {
namespace hdb {

HDBContainer::HDBContainer() = default;
HDBContainer::~HDBContainer() = default;

bool HDBContainer::load_from_file(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    if (sz < static_cast<long>(HEADER_SIZE)) {
        std::fclose(f);
        return false;
    }
    std::fseek(f, 0, SEEK_SET);
    raw_.resize(static_cast<std::size_t>(sz));
    std::size_t n = std::fread(raw_.data(), 1, raw_.size(), f);
    std::fclose(f);
    return n == raw_.size();
}

bool HDBContainer::save_to_file(const std::string& path) const {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::size_t n = std::fwrite(raw_.data(), 1, raw_.size(), f);
    std::fclose(f);
    return n == raw_.size();
}

std::size_t HDBContainer::page_count() const {
    if (raw_.size() < HEADER_SIZE) return 0;
    return (raw_.size() - HEADER_SIZE) / PAGE_SIZE;
}

const Page& HDBContainer::page(std::size_t idx) const {
    return *reinterpret_cast<const Page*>(raw_.data() + HEADER_SIZE + idx * PAGE_SIZE);
}

const uint8_t* HDBContainer::trailer() const {
    return raw_.data() + HEADER_SIZE + page_count() * PAGE_SIZE;
}

std::size_t HDBContainer::trailer_size() const {
    return raw_.size() - HEADER_SIZE - page_count() * PAGE_SIZE;
}

std::size_t hdb_parse_page(const Page& page,
                           std::vector<Record8>* out,
                           std::string* key_string,
                           std::size_t* key_padded_len) {
    if (!out) return 0;
    std::size_t cursor = 0;
    uint8_t first = page.bytes[0];

    // Data page with letter-prefix first byte (A-Z, a-z) — starts with a
    // NUL-terminated key string padded to 8-byte alignment. Mirrors
    // scripts/gam_field_aware_roundtrip.py KeyStringField.
    bool is_letter = ((first >= 0x41 && first <= 0x5a)
                   || (first >= 0x61 && first <= 0x7a));
    if (is_letter) {
        // Find first NUL within the first 64 bytes (Python guard).
        std::size_t nul_pos = PAGE_SIZE;
        for (std::size_t i = 0; i < 64 && i < PAGE_SIZE; ++i) {
            if (page.bytes[i] == 0) { nul_pos = i; break; }
        }
        if (nul_pos < 64) {
            std::size_t pad_end = nul_pos + 1;
            while (pad_end < PAGE_SIZE && page.bytes[pad_end] == 0
                   && (pad_end % 8) != 0) {
                ++pad_end;
            }
            if (key_string) {
                key_string->assign(reinterpret_cast<const char*>(page.bytes),
                                   nul_pos);
            }
            if (key_padded_len) *key_padded_len = pad_end;
            cursor = pad_end;
        }
    }

    // Remaining bytes : sequence of 8-byte typed records (big-endian fields).
    std::size_t added = 0;
    while (cursor + 8 <= PAGE_SIZE) {
        out->push_back(Record8::parse(page.bytes + cursor));
        cursor += 8;
        ++added;
    }
    return added;
}

}  // namespace hdb
}  // namespace xfiles
