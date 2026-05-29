// SPDX-License-Identifier: MIT
// QuickTime container parser — walks the atom tree of *.XMV (= QT MOV) files.
//
// the on-disk format's XMV/NMV/AMV/DMV files are all QuickTime MOV containers. The video
// streams use Cinepak (fourcc 'cvid') ; audio uses IMA ADPCM / QDM2.
//
// This module only parses the atom tree and extracts metadata sufficient to
// locate the compressed video sample bytes. Codec decoding (Cinepak) is in
// a separate module.

#ifndef ASSETS_QT_CONTAINER_H
#define ASSETS_QT_CONTAINER_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace xfiles {
namespace assets {

/// Per-stream summary extracted from stsd / stco / stsz / stts atoms.
struct QtVideoTrack {
    uint32_t fourcc       = 0;   ///< 'cvid' for Cinepak, 'mjpa' for mjpeg, etc.
    uint16_t width        = 0;
    uint16_t height       = 0;
    uint32_t frame_count  = 0;
    uint32_t timescale    = 0;   ///< From mdhd (media time units per second)
    uint32_t total_duration = 0; ///< Sum of all sample durations (from stts)
    /// Approximate frame rate in fps (timescale / mean_frame_duration).
    /// Returns 0 if timing info isn't available.
    float frame_rate() const {
        if (timescale == 0 || total_duration == 0 || frame_count == 0)
            return 0.f;
        float mean_dur = float(total_duration) / float(frame_count);
        return float(timescale) / mean_dur;
    }
    /// File offsets + sizes of each compressed frame in the mdat.
    std::vector<uint32_t> sample_offsets;
    std::vector<uint32_t> sample_sizes;
};

/// Audio track summary. XMV files mostly use 'ima4' (IMA ADPCM
/// QuickTime variant) at 22050 Hz mono.
struct QtAudioTrack {
    uint32_t fourcc        = 0;   ///< 'ima4', 'sowt', 'twos', etc.
    uint16_t channels      = 0;
    uint16_t sample_size   = 0;   ///< Bits per sample (uncompressed)
    uint32_t sample_rate   = 0;   ///< Samples per second (e.g., 22050)
    uint32_t sample_count  = 0;   ///< Number of compressed packets
    uint32_t timescale     = 0;
    /// PCM samples (per channel) to skip from the decoded output, taken from
    /// the trak's edit list (elst mediaTime field). XMVs commonly have
    /// `mediaTime=48` on audio to skip leading silence padding.
    uint32_t edit_skip_samples = 0;
    std::vector<uint32_t> sample_offsets;
    std::vector<uint32_t> sample_sizes;
};

struct QtFile {
    /// Whole file bytes (kept for direct access to samples).
    std::vector<uint8_t> data;
    QtVideoTrack video;
    QtAudioTrack audio;
};

/// Parse a QuickTime MOV (XMV/NMV/AMV/DMV) file. Returns true if a video
/// track was found and metadata extracted.
bool qt_load(const std::string& path, QtFile* out);

/// Get pointer + size for a specific video frame's compressed bytes.
/// Returns nullptr + 0 if index out of range.
const uint8_t* qt_video_sample(const QtFile& f, std::size_t i, std::size_t* out_size);

/// FourCC helper : "cvid" -> 0x63766964.
constexpr uint32_t qt_fourcc(char a, char b, char c, char d) {
    return (uint32_t(uint8_t(a)) << 24)
         | (uint32_t(uint8_t(b)) << 16)
         | (uint32_t(uint8_t(c)) <<  8)
         |  uint32_t(uint8_t(d));
}

}  // namespace assets
}  // namespace xfiles

#endif  // ASSETS_QT_CONTAINER_H
