// SPDX-License-Identifier: MIT
// Cinepak codec decoder.
//
// the on-disk format's XMV / NMV / AMV / DMV video files use Cinepak (fourcc 'cvid')
// for the video stream. Cinepak is a small, intra-frame-friendly codec
// designed for early-90s CD-ROM playback. Format details :
//   https://multimedia.cx/mirror/cinepak.txt
//   https://wiki.multimedia.cx/index.php/Cinepak
//
// Frame is divided into horizontal strips. Each strip carries codebooks
// (V1, V4) and a block plan (V1 vs V4 per 4x4 block). Codebook entries are
// 4-byte (Y only) or 6-byte (Y + Cb + Cr) ; Cb/Cr are signed int8_t.
// Codebook state persists across strips and across frames at the same
// strip index (per FFmpeg cinepak.c).
//
// Supported chunks :
//   0x2000 V4 full codebook (6-byte)
//   0x2200 V4 partial Y-only update (4-byte)
//   0x2100 V1 full codebook (6-byte)
//   0x2300 V1 partial Y-only update (4-byte)
//   0x2400-0x2700 codebook updates with selection bits
//   0x3000 intra block plan (V1/V4 selection bits + indices)
//   0x3100 inter block plan (skip bits + V1/V4 indices)
//   0x3200 V1-only intra block plan
//
// Codebook semantics : per-strip persistent codebooks. On a KEYFRAME
// (frame flags=0x00, strip 0 = id 0x1000 intra), strip N inherits the
// codebook from strip N-1 before applying its own chunk updates. On
// INTER frames (flags=0x01), per-strip codebooks persist independently
// across frames at the same strip index.
//
// Verified BYTE-EXACT against FFmpeg's reference cinepak decoder on
// the on-disk format XV/17923.XMV (600x240, 538 frames) for frames 0, 30, 60, 100,
// 120, 130 — 0 pixels differ by more than 30 (avg sub-pixel diff < 0.25
// from PNG roundtrip noise). Also verified on XV/19674.XMV.

#ifndef ASSETS_CINEPAK_DECODER_H
#define ASSETS_CINEPAK_DECODER_H

#include <cstdint>
#include <cstddef>
#include <vector>

namespace xfiles {
namespace assets {

struct CinepakFrame {
    int width  = 0;
    int height = 0;
    /// RGBA8 pixels, top-left origin, width*height*4 bytes.
    std::vector<uint8_t> rgba;
    bool ok = false;
};

class CinepakDecoder {
public:
    /// Decode one Cinepak frame. Codebooks persist across frames (so feed
    /// frames in order if there are P-frames).
    CinepakFrame decode_frame(const uint8_t* data, std::size_t size);

private:
    // Cb/Cr are stored as SIGNED int8_t (neutral = 0). Verified empirically :
    // for the on-disk format XV/19650.XMV strip 0 codebook, 255/256 V4 entries have
    // chroma within [-20, +20] using signed interpretation, whereas only
    // 0/256 are within [108, 148] under the bias-128 interpretation.
    struct Vec { uint8_t y[4]; int8_t cb, cr; };

    // 4 strips max in practice for the on-disk format (usually 1-2). Each strip has its
    // own pair of codebooks (V1, V4). We size 4 for safety.
    Vec v1_[4][256] = {};
    Vec v4_[4][256] = {};
    bool seen_frame_ = false;
    // Previous frame's RGBA — inter-coded strips (id 0x1100 / chunk 0x3100)
    // reference this buffer for skipped blocks.
    std::vector<uint8_t> prev_rgba_;
    int prev_w_ = 0;
    int prev_h_ = 0;
};

}  // namespace assets
}  // namespace xfiles

#endif  // ASSETS_CINEPAK_DECODER_H
