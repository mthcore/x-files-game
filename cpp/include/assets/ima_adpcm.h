// SPDX-License-Identifier: MIT
// IMA ADPCM (QuickTime variant) decoder — embedded reference decoder.
//
// the on-disk format delegates audio decoding to the OS (MCI -> QuickTime codec).
// Since QuickTime is not available on modern Win11, we embed our own
// decoder. This is NOT a translation of the original code (the on-disk format
// has no IMA decoder in its binary), but engineering work needed to make
// the decoder portable.
//
// Format reference :
//   QuickTime File Format Specification, "Sound Sample Description (IMA)"
//   https://wiki.multimedia.cx/index.php/IMA_ADPCM
//
// Each compressed packet (per channel) :
//   34 bytes = 2-byte preamble + 32 bytes of 4-bit nibbles
//   Preamble (big-endian u16) :
//     - bits 15..7 : initial predictor (signed 9-bit, sign-extended to 16)
//     - bits  6..0 : step index (0..88)
//   Nibbles : 64 samples, LOW nibble of byte first.
//
// Stereo files have packets interleaved per channel : L[0], R[0], L[1], R[1]...

#ifndef ASSETS_IMA_ADPCM_H
#define ASSETS_IMA_ADPCM_H

#include <cstdint>
#include <cstddef>

namespace xfiles {
namespace assets {

constexpr std::size_t IMA_PACKET_BYTES        = 34;
constexpr std::size_t IMA_SAMPLES_PER_PACKET  = 64;

/// Per-channel state carried across consecutive packets. QT IMA encoders
/// optimise the stream by keeping the predictor running across packets when
/// the new preamble is close to the old state ; see FFmpeg adpcm.c
/// (case ADPCM_IMA_QT).
struct ImaChannelState {
    int predictor  = 0;
    int step_index = 0;
};

/// Decode ONE 34-byte IMA ADPCM packet for a single channel.
/// `cs` accumulates predictor/step_index across calls — pass the SAME state
/// for consecutive packets of the same channel. The preamble + state heuristic
/// (matching FFmpeg) keeps the old predictor when the new one is within ±0x7F.
/// Writes 64 int16_t samples to `out`.
void ima_decode_packet(const uint8_t* packet, ImaChannelState* cs, int16_t* out);

/// Convenience : decode a stream of stereo packets into interleaved L/R
/// int16_t samples. `total_packets` is the total packet count (so
/// total/2 packets per channel). Output is `total/2 * 64 * 2` int16_t.
/// Returns number of samples written (not bytes).
std::size_t ima_decode_stereo_stream(const uint8_t* packets,
                                      std::size_t total_packets,
                                      int16_t* out_lr);

/// Mono variant.
std::size_t ima_decode_mono_stream(const uint8_t* packets,
                                    std::size_t total_packets,
                                    int16_t* out);

}  // namespace assets
}  // namespace xfiles

#endif  // ASSETS_IMA_ADPCM_H
