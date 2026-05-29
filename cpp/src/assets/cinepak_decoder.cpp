// SPDX-License-Identifier: MIT
// Cinepak decoder implementation.
//
// Pure C++14, ~200 lines. No external dependencies.
// Tested against the on-disk format XV/19650.XMV (600x240, 512 frames).

#include "assets/cinepak_decoder.h"

#include <cstring>

namespace xfiles {
namespace assets {

namespace {

inline uint16_t be16(const uint8_t* p) {
    return uint16_t((uint16_t(p[0]) << 8) | uint16_t(p[1]));
}
inline uint32_t be24(const uint8_t* p) {
    return (uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | uint32_t(p[2]);
}

inline uint8_t clamp_u8(int v) {
    return uint8_t(v < 0 ? 0 : (v > 255 ? 255 : v));
}

inline void yuv_to_rgba(uint8_t y, int8_t cb, int8_t cr, uint8_t* px) {
    // Cinepak Cb/Cr are SIGNED bytes (neutral = 0). YUV->RGB :
    //   R = Y + 2*Cr
    //   G = Y - Cb/2 - Cr
    //   B = Y + 2*Cb
    int r = int(y) + 2 * int(cr);
    int g = int(y) - (int(cb) >> 1) - int(cr);
    int b = int(y) + 2 * int(cb);
    px[0] = clamp_u8(r);
    px[1] = clamp_u8(g);
    px[2] = clamp_u8(b);
    px[3] = 0xFF;
}

}  // namespace

CinepakFrame CinepakDecoder::decode_frame(const uint8_t* data, std::size_t size) {
    CinepakFrame out;
    if (!data || size < 10) return out;

    // Frame header
    uint8_t  flags       = data[0];
    uint32_t frame_size  = be24(data + 1);
    uint16_t w           = be16(data + 4);
    uint16_t h           = be16(data + 6);
    uint16_t num_strips  = be16(data + 8);
    (void)flags;
    if (frame_size > size) frame_size = static_cast<uint32_t>(size);
    if (w == 0 || h == 0 || w > 4096 || h > 4096 || num_strips > 4) return out;

    out.width  = w;
    out.height = h;
    // Start from the previous frame's contents (so inter-frame skip blocks
    // are correct). If the previous frame has a different size or we have
    // no previous frame, fill with black.
    if (prev_w_ == w && prev_h_ == h
        && prev_rgba_.size() == static_cast<std::size_t>(w * h * 4)) {
        out.rgba = prev_rgba_;
    } else {
        out.rgba.assign(static_cast<std::size_t>(w * h * 4), 0);
        for (std::size_t i = 3; i < out.rgba.size(); i += 4) out.rgba[i] = 0xFF;
    }

    std::size_t c = 10;
    int strip_y_offset = 0;
    for (int s = 0; s < num_strips && c + 12 <= frame_size; ++s) {
        uint16_t strip_id   = be16(data + c);
        uint16_t strip_size = be16(data + c + 2);
        uint16_t s_y0       = be16(data + c + 4);
        /*uint16_t s_x0     =*/ be16(data + c + 6);
        uint16_t s_y1       = be16(data + c + 8);
        uint16_t s_x1       = be16(data + c + 10);

        if (strip_size < 12 || c + strip_size > frame_size) break;

        // Per-spec strip y is RELATIVE to previous strip's bottom. We treat
        // s_y0/s_y1 as a height range (0..H) relative to strip_y_offset.
        int s_h = s_y1 - s_y0;
        int s_w = s_x1;
        if (s_h <= 0) s_h = h - strip_y_offset;
        if (s_w <= 0) s_w = w;
        int strip_top  = strip_y_offset;
        int strip_left = 0;

        // For intra strips (0x1000) the codebooks have NOT been initialised
        // for this strip — caller relies on previous state on inter (0x1100).

        // Walk chunks within the strip
        std::size_t cc = c + 12;
        std::size_t cend = c + strip_size;

        // Codebook semantics per FFmpeg cinepak.c :
        //   - Each strip has its own V1 / V4 codebook, persistent across frames.
        //   - On a KEYFRAME (flags=0x00, strip 0 = 0x1000 intra), before
        //     processing strip N (N > 0), the codebook from strip N-1 is
        //     COPIED into strip N. The strip's own chunks then apply on top.
        //   - On an INTER frame (flags=0x01), per-strip codebook persists
        //     without inheritance.
        int slot = s & 3;
        if (s > 0 && (flags & 0x01) == 0) {
            int prev = (s - 1) & 3;
            std::memcpy(v1_[slot], v1_[prev], sizeof(Vec) * 256);
            std::memcpy(v4_[slot], v4_[prev], sizeof(Vec) * 256);
        }
        Vec* v1 = v1_[slot];
        Vec* v4 = v4_[slot];

        // Block plan : 256-bit selection map per row of 32 blocks (= 128 px wide).
        // We'll walk blocks ourselves consuming index/selection chunks.
        struct BlockChunk {
            uint16_t id = 0;
            const uint8_t* data = nullptr;
            std::size_t size = 0;
        };
        BlockChunk block_chunk{};

        while (cc + 4 <= cend) {
            uint16_t chunk_id   = be16(data + cc);
            uint16_t chunk_size = be16(data + cc + 2);
            if (chunk_size < 4 || cc + chunk_size > cend) break;
            const uint8_t* body = data + cc + 4;
            std::size_t   blen  = chunk_size - 4;

            switch (chunk_id) {
            case 0x2000: case 0x2100: case 0x2200: case 0x2300:
            case 0x2400: case 0x2500: case 0x2600: case 0x2700: {
                // Per multimedia.cx Cinepak spec, the codebook chunk ID encodes :
                //   bit 8 (0x0100) : inter (1, selection-bit) vs intra (0, full)
                //   bit 9 (0x0200) : V1   (1)                  vs V4   (0)
                //   bit 10 (0x0400) : Y-only / 4-byte (1)      vs 6-byte (0)
                bool is_inter   = (chunk_id & 0x0100) != 0;
                bool is_v1      = (chunk_id & 0x0200) != 0;
                bool is_4byte   = (chunk_id & 0x0400) != 0;
                std::size_t entry_size = is_4byte ? 4 : 6;
                bool has_chroma = !is_4byte;
                Vec* cb_arr = is_v1 ? v1 : v4;
                if (!is_inter) {
                    // Intra : full codebook, all entries packed.
                    std::size_t n = blen / entry_size;
                    if (n > 256) n = 256;
                    for (std::size_t i = 0; i < n; ++i) {
                        const uint8_t* e = body + i * entry_size;
                        cb_arr[i].y[0] = e[0]; cb_arr[i].y[1] = e[1];
                        cb_arr[i].y[2] = e[2]; cb_arr[i].y[3] = e[3];
                        if (has_chroma) {
                            cb_arr[i].cb = static_cast<int8_t>(e[4]);
                            cb_arr[i].cr = static_cast<int8_t>(e[5]);
                        }
                    }
                } else {
                    // Inter : 32-bit selection word + N entries, repeated 8x
                    // to cover all 256 entries. A set bit means "next entry
                    // in the stream replaces this codebook slot".
                    std::size_t bp = 0;
                    for (std::size_t group = 0; group < 8; ++group) {
                        if (bp + 4 > blen) break;
                        uint32_t sel = (uint32_t(body[bp + 0]) << 24)
                                     | (uint32_t(body[bp + 1]) << 16)
                                     | (uint32_t(body[bp + 2]) <<  8)
                                     |  uint32_t(body[bp + 3]);
                        bp += 4;
                        for (int bit = 0; bit < 32; ++bit) {
                            std::size_t i = group * 32 + bit;
                            if (sel & (1u << (31 - bit))) {
                                if (bp + entry_size > blen) break;
                                const uint8_t* e = body + bp;
                                cb_arr[i].y[0] = e[0]; cb_arr[i].y[1] = e[1];
                                cb_arr[i].y[2] = e[2]; cb_arr[i].y[3] = e[3];
                                if (has_chroma) {
                                    cb_arr[i].cb = static_cast<int8_t>(e[4]);
                                    cb_arr[i].cr = static_cast<int8_t>(e[5]);
                                }
                                bp += entry_size;
                            }
                        }
                    }
                }
                break;
            }
            case 0x3000: case 0x3100: case 0x3200:
                block_chunk.id = chunk_id;
                block_chunk.data = body;
                block_chunk.size = blen;
                break;
            default:
                // Unknown chunk - skip
                break;
            }
            cc += chunk_size;
        }

        if (block_chunk.id != 0) {
            // Render blocks
            int bw = (s_w + 3) / 4;
            int bh = (s_h + 3) / 4;
            const uint8_t* bd = block_chunk.data;
            std::size_t bsize = block_chunk.size;
            std::size_t bp = 0;
            uint32_t sel_word = 0;
            int sel_bits = 0;

            auto next_sel = [&]() -> int {
                if (block_chunk.id == 0x3200) return 0;  // V1 only
                if (sel_bits == 0) {
                    if (bp + 4 > bsize) return -1;
                    sel_word = (uint32_t(bd[bp]) << 24) | (uint32_t(bd[bp+1]) << 16)
                             | (uint32_t(bd[bp+2]) <<  8) |  uint32_t(bd[bp+3]);
                    bp += 4;
                    sel_bits = 32;
                }
                int b = (sel_word >> 31) & 1;
                sel_word <<= 1;
                --sel_bits;
                return b;
            };

            // Block plan bitstream interpretation (per multimedia.cx spec) :
            //   0x3000 (intra) : 1 bit per MB. 0=V1 (1 index byte),
            //                                 1=V4 (4 index bytes).
            //   0x3100 (inter) : variable-length, 1-2 bits per MB :
            //                    0  = skip (preserve previous frame's MB),
            //                    10 = V1 (1 index byte follows),
            //                    11 = V4 (4 index bytes follow).
            //                    BOTH bits live in the same bitstream (refilled
            //                    in 32-bit words on demand).
            //   0x3200 : no selection bits, all V1.
            for (int by = 0; by < bh; ++by) {
                for (int bx = 0; bx < bw; ++bx) {
                    int use_v4;
                    if (block_chunk.id == 0x3200) {
                        use_v4 = 0;
                    } else if (block_chunk.id == 0x3100) {
                        // 1st bit : changed (1) or skipped (0)
                        int changed = next_sel();
                        if (changed < 0) goto strip_done;
                        if (changed == 0) continue;
                        // 2nd bit : V4 (1) or V1 (0)
                        int v4_bit = next_sel();
                        if (v4_bit < 0) goto strip_done;
                        use_v4 = v4_bit;
                    } else {
                        // 0x3000 : standard intra, single bitstream.
                        use_v4 = next_sel();
                        if (use_v4 < 0) goto strip_done;
                    }
                    int px = strip_left + bx * 4;
                    int py = strip_top  + by * 4;
                    if (use_v4) {
                        // 4 indices, one per 2x2 sub-block. Order in stream :
                        // top-left, top-right, bottom-left, bottom-right (raster).
                        if (bp + 4 > bsize) goto strip_done;
                        uint8_t i_tl = bd[bp+0], i_tr = bd[bp+1];
                        uint8_t i_bl = bd[bp+2], i_br = bd[bp+3];
                        bp += 4;
                        // Map quadrant index (raster qi=0..3 = TL,TR,BL,BR) to vectors.
                        const Vec* q[4] = { &v4[i_tl], &v4[i_tr], &v4[i_bl], &v4[i_br] };
                        // Sub-block layout : q[0]=top-left, q[1]=top-right,
                        //                    q[2]=bot-left, q[3]=bot-right
                        for (int qi = 0; qi < 4; ++qi) {
                            int sx = px + (qi & 1) * 2;
                            int sy = py + (qi >> 1) * 2;
                            const Vec& v = *q[qi];
                            for (int dy = 0; dy < 2; ++dy) {
                                for (int dx = 0; dx < 2; ++dx) {
                                    int x = sx + dx, y = sy + dy;
                                    if (x >= w || y >= h) continue;
                                    yuv_to_rgba(v.y[dy * 2 + dx], v.cb, v.cr,
                                                out.rgba.data() + (y * w + x) * 4);
                                }
                            }
                        }
                    } else {
                        // 1 index, V1 vector
                        if (bp + 1 > bsize) goto strip_done;
                        uint8_t i = bd[bp++];
                        const Vec& v = v1[i];
                        // Map : top-left 2x2 = Y0, top-right = Y1,
                        //       bot-left   = Y2, bot-right    = Y3
                        for (int qi = 0; qi < 4; ++qi) {
                            int sx = px + (qi & 1) * 2;
                            int sy = py + (qi >> 1) * 2;
                            uint8_t yv = v.y[qi];
                            for (int dy = 0; dy < 2; ++dy) {
                                for (int dx = 0; dx < 2; ++dx) {
                                    int x = sx + dx, y = sy + dy;
                                    if (x >= w || y >= h) continue;
                                    yuv_to_rgba(yv, v.cb, v.cr,
                                                out.rgba.data() + (y * w + x) * 4);
                                }
                            }
                        }
                    }
                }
            }
strip_done:;
        }

        strip_y_offset += s_h;
        c += strip_size;
    }

    seen_frame_ = true;
    out.ok = true;
    // Stash for next inter frame.
    prev_rgba_ = out.rgba;
    prev_w_ = w;
    prev_h_ = h;
    return out;
}

}  // namespace assets
}  // namespace xfiles
