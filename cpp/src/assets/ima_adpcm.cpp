// SPDX-License-Identifier: MIT
// IMA ADPCM (QuickTime variant) decoder. Mirrors FFmpeg's adpcm.c verbatim
// for AV_CODEC_ID_ADPCM_IMA_QT — bit-for-bit, including the predictor
// state-carryover heuristic between consecutive packets of the same channel.

#include "assets/ima_adpcm.h"

#include <cstring>

namespace xfiles {
namespace assets {

namespace {

// Standard IMA ADPCM step size table (89 entries) — public domain.
constexpr int16_t kStepTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

constexpr int8_t kIndexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

inline int clamp_i16(int v) {
    if (v >  32767) return  32767;
    if (v < -32768) return -32768;
    return v;
}

inline int clamp_index(int idx) {
    if (idx < 0)  return 0;
    if (idx > 88) return 88;
    return idx;
}

inline int sign_extend_16(uint32_t v) {
    return static_cast<int>(static_cast<int16_t>(v));
}

inline int16_t expand_nibble(ImaChannelState* cs, int nibble) {
    int step = kStepTable[cs->step_index];
    int new_step_idx = clamp_index(cs->step_index + kIndexTable[nibble]);
    int diff = step >> 3;
    if (nibble & 4) diff += step;
    if (nibble & 2) diff += step >> 1;
    if (nibble & 1) diff += step >> 2;
    if (nibble & 8) cs->predictor -= diff;
    else            cs->predictor += diff;
    cs->predictor = clamp_i16(cs->predictor);
    cs->step_index = new_step_idx;
    return static_cast<int16_t>(cs->predictor);
}

}  // namespace

void ima_decode_packet(const uint8_t* p, ImaChannelState* cs, int16_t* out) {
    // Preamble : big-endian u16. Bits 15..7 = predictor, bits 6..0 = step idx.
    uint32_t raw = (uint32_t(p[0]) << 8) | uint32_t(p[1]);
    int new_predictor  = sign_extend_16(raw);     // full 16-bit sign-extend
    int new_step_index = new_predictor & 0x7F;
    new_predictor &= ~0x7F;                       // clear bottom 7 bits

    // FFmpeg state-carryover : if the new step index matches the running
    // state and the new predictor is within ±0x7F of the running predictor,
    // keep decoding from the running state. Otherwise reseed.
    bool reseed = true;
    if (cs->step_index == new_step_index) {
        int diff = new_predictor - cs->predictor;
        if (diff < 0) diff = -diff;
        reseed = (diff > 0x7F);
    }
    if (reseed) {
        cs->predictor  = new_predictor;
        cs->step_index = new_step_index;
    }
    // Step index must be in [0,88] regardless of origin.
    cs->step_index = clamp_index(cs->step_index);

    // Decode 32 nibble-pair bytes -> 64 samples. Low nibble first.
    int out_i = 0;
    for (int i = 0; i < 32; ++i) {
        uint8_t byte = p[2 + i];
        out[out_i++] = expand_nibble(cs, byte & 0x0F);
        out[out_i++] = expand_nibble(cs, (byte >> 4) & 0x0F);
    }
}

std::size_t ima_decode_mono_stream(const uint8_t* packets,
                                    std::size_t total_packets,
                                    int16_t* out) {
    ImaChannelState cs;
    for (std::size_t i = 0; i < total_packets; ++i) {
        ima_decode_packet(packets + i * IMA_PACKET_BYTES,
                          &cs,
                          out + i * IMA_SAMPLES_PER_PACKET);
    }
    return total_packets * IMA_SAMPLES_PER_PACKET;
}

std::size_t ima_decode_stereo_stream(const uint8_t* packets,
                                      std::size_t total_packets,
                                      int16_t* out_lr) {
    // Stereo : packets alternate L,R,L,R per frame. Per-channel state is
    // independent and carries across consecutive packets of that channel.
    std::size_t pair_count = total_packets / 2;
    ImaChannelState cs_l, cs_r;
    int16_t tmp_l[IMA_SAMPLES_PER_PACKET];
    int16_t tmp_r[IMA_SAMPLES_PER_PACKET];
    for (std::size_t i = 0; i < pair_count; ++i) {
        ima_decode_packet(packets + (2 * i + 0) * IMA_PACKET_BYTES, &cs_l, tmp_l);
        ima_decode_packet(packets + (2 * i + 1) * IMA_PACKET_BYTES, &cs_r, tmp_r);
        for (std::size_t s = 0; s < IMA_SAMPLES_PER_PACKET; ++s) {
            out_lr[(i * IMA_SAMPLES_PER_PACKET + s) * 2 + 0] = tmp_l[s];
            out_lr[(i * IMA_SAMPLES_PER_PACKET + s) * 2 + 1] = tmp_r[s];
        }
    }
    return pair_count * IMA_SAMPLES_PER_PACKET * 2;
}

}  // namespace assets
}  // namespace xfiles
