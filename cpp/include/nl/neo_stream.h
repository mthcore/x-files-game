// SPDX-License-Identifier: MIT
// CNeoFileStream wire format (NeoPersist 3.0 on-disk encoding).
//
// This is the REAL wire format X-Files uses on disk, distinct from the
// synthetic TLV format implemented by InMemoryHDBSerializer (which was
// designed for unit tests only). The differences :
//
//   - Endianness     : BIG-ENDIAN (NeoAccess canonical Mac-origin)
//   - Tag validation : NONE inline (the v4>4 version check is FALSE
//                      in X-Files since CNeoFormat+20 = 0)
//   - Marker format  : [NPfl 2B][NPvr 4B BE][class_id 2B BE] = 8 bytes
//                      = CNeoPersist on-disk header per CNeoPersist.h
//
// Primitive readers :
//   readChunk_low : buffered N-byte read (4096B IOBlock)
//   readLong      : 4B BE + bswap32
//   bswap32       : full 4-byte byte-swap
//   reallyReadChunk : SetFilePointer + ReadFile direct
//
// CNeoFileStream instance layout :
//   +0x04 = fMark (current file position)
//   +0xC  = fInputFormat (CNeoFormat*)
//   +0xD4 = HANDLE fFile (Win32 file handle)
//   +0x14 = fIOBlock cache (4096B buffer)

#ifndef NL_NEO_STREAM_H
#define NL_NEO_STREAM_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <stdexcept>

namespace xfiles {
namespace nl {

/// Decoded CNeoPersist on-disk header (the 8-byte "marker" preceding every
/// persistent record in the HDB).
struct NeoPersistMarker {
    uint16_t packed_flags;   // NPfl bit-field
    uint32_t version;        // NPvr (= 1 in X-Files = kNeoVersionDefault)
    uint16_t class_id;       // Class identifier (0x00-0x5C)

    // Decoded NPfl bits per CNeoPersist.h
    bool      f_leaf()        const { return (packed_flags & 0x0001) != 0; }
    bool      f_root()        const { return (packed_flags & 0x0002) != 0; }
    bool      f_temporary()   const { return (packed_flags & 0x0004) != 0; }
    uint8_t   f_local()       const { return (packed_flags >> 3) & 0x03; }
    bool      f_share_fill()  const { return (packed_flags & 0x0020) != 0; }
    bool      f_busy()        const { return (packed_flags & 0x0040) != 0; }
    bool      f_bool1()       const { return (packed_flags & 0x0080) != 0; }
    bool      f_bool2()       const { return (packed_flags & 0x0100) != 0; }
    bool      f_bool3()       const { return (packed_flags & 0x0200) != 0; }
    uint8_t   f_peer_dirty()  const { return (packed_flags >> 12) & 0x03; }
    uint8_t   f_dirty()       const { return (packed_flags >> 14) & 0x03; }
};

/// Byte-direct reader for NeoAccess HDB records.
/// Mirrors CNeoFileStream behaviour, decoded byte-direct.
class NeoStream {
public:
    NeoStream(const uint8_t* data, std::size_t size)
        : data_(data), size_(size), cursor_(0) {}

    /// Reset cursor to a specific position. Equivalent to setMark(NeoMark).
    void set_mark(std::size_t pos) {
        if (pos > size_) throw std::out_of_range("set_mark beyond buffer");
        cursor_ = pos;
    }

    std::size_t mark() const { return cursor_; }
    std::size_t size() const { return size_; }
    const uint8_t* data() const { return data_; }
    bool eof() const { return cursor_ >= size_; }

    /// raw N-byte read with bounds check.
    void read_chunk(void* dest, std::size_t n) {
        if (cursor_ + n > size_) {
            throw std::out_of_range("read_chunk past end");
        }
        std::memcpy(dest, data_ + cursor_, n);
        cursor_ += n;
    }

    /// 1 unsigned byte.
    uint8_t read_byte() {
        if (cursor_ >= size_) throw std::out_of_range("read_byte past end");
        return data_[cursor_++];
    }

    /// 1 byte → bool (non-zero = true).
    bool read_boolean() {
        return read_byte() != 0;
    }

    /// 2B BE → u16.
    uint16_t read_short() {
        if (cursor_ + 2 > size_) throw std::out_of_range("read_short");
        uint16_t v = static_cast<uint16_t>(data_[cursor_]) << 8
                   | data_[cursor_ + 1];
        cursor_ += 2;
        return v;
    }

    int16_t read_short_signed() {
        return static_cast<int16_t>(read_short());
    }

    /// 4B BE → u32 (full bswap32).
    uint32_t read_long() {
        if (cursor_ + 4 > size_) throw std::out_of_range("read_long");
        uint32_t v = static_cast<uint32_t>(data_[cursor_])     << 24
                   | static_cast<uint32_t>(data_[cursor_ + 1]) << 16
                   | static_cast<uint32_t>(data_[cursor_ + 2]) << 8
                   |                       data_[cursor_ + 3];
        cursor_ += 4;
        return v;
    }

    int32_t read_long_signed() {
        return static_cast<int32_t>(read_long());
    }

    /// Pascal string : 1B length + chars (Mac classic CNeoString).
    /// Per CNeoStream.cp readNativeString.
    std::string read_native_string(std::size_t max_len = 255) {
        uint8_t n = read_byte();
        if (n > max_len) n = static_cast<uint8_t>(max_len);
        if (cursor_ + n > size_) throw std::out_of_range("read_native_string");
        std::string s(reinterpret_cast<const char*>(data_ + cursor_), n);
        cursor_ += n;
        return s;
    }

    /// Read the CNeoPersist on-disk header (= the 8-byte record marker).
    /// Format : [NPfl 2B][NPvr 4B BE][class_id 2B BE]
    NeoPersistMarker read_marker() {
        NeoPersistMarker m;
        m.packed_flags = read_short();
        m.version      = read_long();
        m.class_id     = read_short();
        return m;
    }

    /// Peek a marker WITHOUT advancing cursor.
    NeoPersistMarker peek_marker() const {
        if (cursor_ + 8 > size_) throw std::out_of_range("peek_marker");
        NeoPersistMarker m;
        m.packed_flags = static_cast<uint16_t>(data_[cursor_]) << 8
                       | data_[cursor_ + 1];
        m.version      = static_cast<uint32_t>(data_[cursor_ + 2]) << 24
                       | static_cast<uint32_t>(data_[cursor_ + 3]) << 16
                       | static_cast<uint32_t>(data_[cursor_ + 4]) << 8
                       |                       data_[cursor_ + 5];
        m.class_id     = static_cast<uint16_t>(data_[cursor_ + 6]) << 8
                       | data_[cursor_ + 7];
        return m;
    }

    /// Returns true if the bytes at `off` look like a valid record marker
    /// (magic NPvr=1 at offset +2..+5).
    static bool is_marker(const uint8_t* buf, std::size_t buf_size,
                          std::size_t off) {
        if (off + 8 > buf_size) return false;
        return buf[off + 2] == 0x00 && buf[off + 3] == 0x00
            && buf[off + 4] == 0x00 && buf[off + 5] == 0x01;
    }

private:
    const uint8_t* data_;
    std::size_t    size_;
    std::size_t    cursor_;
};

// ===== NeoAccess internal class IDs (NeoTypes.h confirmed) =====
constexpr uint16_t kClassNeoNullClass     = 0x00;
constexpr uint16_t kClassNeoPersist       = 0x01;
constexpr uint16_t kClassNeoBlob          = 0x02;
constexpr uint16_t kClassNeoClass         = 0x03;
constexpr uint16_t kClassNeoSubclass      = 0x04;
constexpr uint16_t kClassNeoFreeList      = 0x05;
constexpr uint16_t kClassNeoInode         = 0x06;
constexpr uint16_t kClassNeoIDIndex       = 0x07;
constexpr uint16_t kClassNeoParentIndex   = 0x08;
constexpr uint16_t kClassNeoGenericIndex  = 0x09;
constexpr uint16_t kClassNeoPartMgr       = 0x0A;
constexpr uint16_t kClassNeoIDList        = 0x0B;

// ===== HyperBole-specific NeoLogic subclasses (0x0F-0x1E inferred) =====
constexpr uint16_t kClassHBListNode    = 0x0F;
constexpr uint16_t kClassHBStringRef   = 0x10;
constexpr uint16_t kClassHBSubScope    = 0x11;
constexpr uint16_t kClassHBExpression  = 0x14;
constexpr uint16_t kClassHBAssetRef    = 0x15;
constexpr uint16_t kClassHBActionItem  = 0x1B;
constexpr uint16_t kClassHBStringTag   = 0x1C;
constexpr uint16_t kClassHBActionBody  = 0x1D;
constexpr uint16_t kClassHBTargetSpec  = 0x1E;

// ===== VC* application classes (0x27-0x5C) =====
constexpr uint16_t kClassVCTitle           = 0x27;
constexpr uint16_t kClassVCNode            = 0x28;
constexpr uint16_t kClassVCLocaton         = 0x29;
constexpr uint16_t kClassVCViewPoint       = 0x2A;
constexpr uint16_t kClassVCView            = 0x2B;
constexpr uint16_t kClassVCCharacter       = 0x2C;
constexpr uint16_t kClassVCCharView        = 0x2D;
constexpr uint16_t kClassVCName            = 0x30;
constexpr uint16_t kClassVCConversation    = 0x31;
constexpr uint16_t kClassVCNav             = 0x33;
constexpr uint16_t kClassVCNavList         = 0x34;
constexpr uint16_t kClassVCAssetRef        = 0x35;
constexpr uint16_t kClassVCExplorable      = 0x37;
constexpr uint16_t kClassVCHotSpot         = 0x39;
constexpr uint16_t kClassVCAction          = 0x41;
constexpr uint16_t kClassVCBlob            = 0x43;
constexpr uint16_t kClassVCPlayer          = 0x44;
constexpr uint16_t kClassVCEnabled         = 0x45;
constexpr uint16_t kClassVCIFaceLayout     = 0x4E;
constexpr uint16_t kClassVCString          = 0x50;
constexpr uint16_t kClassVCTrigger         = 0x51;
constexpr uint16_t kClassVCTriggerList     = 0x52;
constexpr uint16_t kClassVCVariable        = 0x53;
constexpr uint16_t kClassVCStdAction       = 0x54;
constexpr uint16_t kClassVCGameState       = 0x57;
constexpr uint16_t kClassVCPhoto           = 0x59;

// ===== Page constants =====
constexpr std::size_t kHdbFileHeaderSize = 32;     // 32-byte file prefix
constexpr std::size_t kHdbPageSize       = 256;    // kNeoDatabaseHeadSpace
constexpr std::size_t kHdbQuantum        = 8;      // kNeoDatabaseQuantum
constexpr uint8_t     kMarkerVersionByte = 0x01;   // NPvr default = 1

}  // namespace nl
}  // namespace xfiles

#endif  // NL_NEO_STREAM_H
