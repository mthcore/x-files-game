// SPDX-License-Identifier: MIT
// PICT decoder — extracts a decoded RGBA bitmap from an Apple QuickDraw PICT.
//
// the on-disk format's X.PFF entries are header-less PICT v2 files. Distribution of bitmap
// opcodes (W25 walker on 727 entries) :
//   - 0x8200 CompressedQuickTime (JPEG embedded) : 372 / 723
//   - 0x009A DirectBitsRect (direct color)       : 347 / 723
//   - 0x0098 PackBitsRect (indexed color)        :  30 / 723
//
// Current support : JPEG via stb_image (covers ~51% of entries). Other
// opcodes are recognised but not yet decoded (caller gets width/height + a
// status code).

#ifndef ASSETS_PICT_DECODER_H
#define ASSETS_PICT_DECODER_H

#include <cstdint>
#include <cstddef>
#include <vector>

namespace xfiles {
namespace assets {

enum class PictResult {
    OK,
    EMPTY,
    TRUNCATED,
    UNSUPPORTED_BITMAP,   ///< Recognised a bitmap opcode but decoder not yet implemented
    JPEG_DECODE_FAILED,
    NO_BITMAP,
};

struct DecodedPict {
    int width  = 0;
    int height = 0;
    /// RGBA8 pixels, top-left origin, row-major, width*height*4 bytes.
    std::vector<uint8_t> rgba;
    PictResult result = PictResult::EMPTY;
    /// First bitmap opcode encountered (0 if none).
    uint16_t bitmap_opcode = 0;
};

/// Decode a PICT bytestream (PFF entry).
DecodedPict decode_pict(const uint8_t* data, std::size_t size);

/// Decode a raw JPEG bytestream to RGBA8. Used both for PICT-embedded JPEGs
/// and for Motion-JPEG NMV samples (NAV*.NMV / NAVM.NMV).
/// On failure returns an empty DecodedPict with result=JPEG_DECODE_FAILED.
DecodedPict decode_jpeg_rgba(const uint8_t* data, std::size_t size);

}  // namespace assets
}  // namespace xfiles

#endif  // ASSETS_PICT_DECODER_H
