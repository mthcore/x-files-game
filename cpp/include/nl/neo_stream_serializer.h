// SPDX-License-Identifier: MIT
// NeoStream-based HDBSerializer : adapter that exposes the existing
// HDBSerializer interface but reads byte-direct from a NeoStream backed
// by real HDB bytes (big-endian, no inline TLV tags).
//
// This is the bridge between :
//   - InMemoryHDBSerializer (synthetic TLV, LE, tag validation)
//   - The REAL X-Files HDB wire format (BE, positional, no inline tags)
//
// All read_* methods take a `tag` arg per the legacy interface but IGNORE
// it (tags are runtime hints only — they don't appear on disk in X-Files
// version of NeoAccess, per the on-disk format's `v4 > 4` always-false check).
//
// All numeric reads are BIG-ENDIAN (canonical NeoAccess Mac-origin format)
// matching bswap32 in the on-disk format's +.

#ifndef NL_NEO_STREAM_SERIALIZER_H
#define NL_NEO_STREAM_SERIALIZER_H

#include "nl/hdb_serializer.h"
#include "nl/neo_stream.h"

namespace xfiles {
namespace nl {

/// Adapter : implements HDBSerializer interface via byte-direct NeoStream.
///
/// Use this to feed REAL HDB record payloads to existing VC*::Read functions
/// without rewriting them. Construct over the payload bytes (the 8-byte
/// CNeoPersist marker is NOT part of the payload — it's consumed separately
/// by the record loader before invoking VCObject::Read).
class NeoStreamHDBSerializer : public HDBSerializer {
public:
    /// Construct over a record payload (post-marker bytes).
    NeoStreamHDBSerializer(const uint8_t* data, std::size_t size)
        : stream_(data, size), error_(false), version_(1),
          has_version_(true) {}

    // ===== Read methods - all ignore the tag parameter =====

    std::string read_string(uint8_t /*tag*/) override {
        try {
            return stream_.read_native_string();
        } catch (...) {
            error_ = true;
            return {};
        }
    }

    void read_buf_fourcc(void* dest, std::size_t count,
                          uint32_t /*fourcc*/) override {
        try {
            stream_.read_chunk(dest, count);
        } catch (...) {
            error_ = true;
        }
    }

    uint8_t read_byte_alt(uint8_t /*tag*/) override {
        try {
            return stream_.read_byte();
        } catch (...) {
            error_ = true;
            return 0;
        }
    }

    uint8_t read_byte(uint8_t /*tag*/) override {
        try {
            return stream_.read_byte();
        } catch (...) {
            error_ = true;
            return 0;
        }
    }

    uint32_t read_dword(uint8_t /*tag*/) override {
        try {
            return stream_.read_long();
        } catch (...) {
            error_ = true;
            return 0;
        }
    }

    void begin_record() override {
        // No-op : the marker is read by the loader, not by Read methods.
    }

    int32_t read_short(uint8_t /*tag*/) override {
        try {
            return stream_.read_short_signed();
        } catch (...) {
            error_ = true;
            return 0;
        }
    }

    uint32_t read_dword_fourcc(uint32_t /*fourcc*/) override {
        try {
            return stream_.read_long();
        } catch (...) {
            error_ = true;
            return 0;
        }
    }

    bool error() const override { return error_; }
    std::size_t cursor() const override { return stream_.mark(); }
    uint32_t current_version() const override { return version_; }
    bool has_version_field() const override { return has_version_; }

    void set_version(uint32_t v) { version_ = v; }
    void set_has_version_field(bool h) { has_version_ = h; }

    // ===== Write methods - not implemented (read-only path) =====

    void write_buf_fourcc(const void* /*src*/, std::size_t /*count*/,
                          uint32_t /*fourcc*/) override {}
    void write_byte(uint8_t /*tag*/, uint8_t /*v*/) override {}
    void write_short(uint8_t /*tag*/, int32_t /*v*/) override {}
    void write_dword(uint8_t /*tag*/, uint32_t /*v*/) override {}
    void write_dword_fourcc(uint32_t /*fourcc*/, uint32_t /*v*/) override {}
    void write_string(uint8_t /*tag*/, const std::string& /*s*/) override {}

    /// Direct access to the underlying stream (for byte-direct ops outside
    /// the HDBSerializer interface).
    NeoStream& stream() { return stream_; }
    const NeoStream& stream() const { return stream_; }

private:
    NeoStream stream_;
    bool error_;
    uint32_t version_;
    bool has_version_;
};

}  // namespace nl
}  // namespace xfiles

#endif  // NL_NEO_STREAM_SERIALIZER_H
