// SPDX-License-Identifier: MIT
// PICT decoder implementation.

#include "assets/pict_decoder.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_GIF
#define STBI_NO_BMP
#define STBI_NO_PNG  // only JPEG needed
#define STB_IMAGE_STATIC
#include "stb_image.h"

#include <cstring>

namespace xfiles {
namespace assets {

namespace {

inline uint16_t be16(const uint8_t* p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}
inline uint32_t be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24)
         | (static_cast<uint32_t>(p[1]) << 16)
         | (static_cast<uint32_t>(p[2]) << 8)
         |  static_cast<uint32_t>(p[3]);
}
inline int16_t  be_i16(const uint8_t* p) { return static_cast<int16_t>(be16(p)); }

/// PackBits decompression (Apple) — reads at most `in_size` bytes from `in`,
/// writes exactly `out_size` decompressed bytes to `out`.
/// Returns the number of input bytes consumed, or 0 on error.
std::size_t unpack_bits(const uint8_t* in, std::size_t in_size,
                        uint8_t* out, std::size_t out_size) {
    std::size_t ip = 0, op = 0;
    while (op < out_size && ip < in_size) {
        uint8_t h = in[ip++];
        if (h == 0x80) continue;  // no-op
        if (h < 0x80) {
            std::size_t n = static_cast<std::size_t>(h) + 1;
            if (ip + n > in_size || op + n > out_size) return 0;
            std::memcpy(out + op, in + ip, n);
            ip += n; op += n;
        } else {
            std::size_t n = 257u - static_cast<std::size_t>(h);
            if (ip >= in_size || op + n > out_size) return 0;
            uint8_t v = in[ip++];
            std::memset(out + op, v, n);
            op += n;
        }
    }
    return (op == out_size) ? ip : 0;
}

/// Try to find a JPEG SOI marker (0xFFD8) within the QuickTime payload.
/// CompressedQuickTime wraps : u32 chunkSize, ImageDescription struct, then
/// the actual image stream. Rather than parse the full QT header (variable),
/// we scan for SOI.
std::size_t find_jpeg_soi(const uint8_t* p, std::size_t n) {
    for (std::size_t i = 0; i + 1 < n; ++i) {
        if (p[i] == 0xFF && p[i+1] == 0xD8 && i + 3 < n && p[i+2] == 0xFF) {
            return i;
        }
    }
    return n;  // not found
}

/// Decode the PackBitsRect opcode body (indexed 8-bit pixmap + ColorTable).
bool decode_packbits_rect(const uint8_t* data, std::size_t size, std::size_t& c,
                          DecodedPict* out, bool is_region) {
    auto need = [&](std::size_t k) -> bool { return c + k <= size; };
    // PackBitsRect has NO baseAddr (DirectBitsRect does).
    if (!need(46)) return false;
    uint16_t rowBytes = be16(data + c) & 0x3FFF; c += 2;
    int16_t  top    = be_i16(data + c); c += 2;
    int16_t  left   = be_i16(data + c); c += 2;
    int16_t  bottom = be_i16(data + c); c += 2;
    int16_t  right  = be_i16(data + c); c += 2;
    /*pmVersion*/   c += 2;
    uint16_t packType = be16(data + c); c += 2;
    /*packSize*/    c += 4;
    c += 8;  // hRes + vRes
    /*pixelType*/   c += 2;
    uint16_t pixelSize = be16(data + c); c += 2;
    /*cmpCount*/    c += 2;
    /*cmpSize*/     c += 2;
    c += 4 + 4 + 4;  // planeBytes + pmTable + pmReserved

    // ColorTable : u32 ctSeed + u16 ctFlags + u16 ctSize + (ctSize+1) * 8-byte ColorSpec
    if (!need(8)) return false;
    c += 4;  // ctSeed
    c += 2;  // ctFlags
    uint16_t ctSize = be16(data + c); c += 2;
    int num_colors = static_cast<int>(ctSize) + 1;
    if (num_colors <= 0 || num_colors > 256) return false;
    if (!need(static_cast<std::size_t>(num_colors) * 8)) return false;
    uint8_t palette[256][4];  // RGBA
    std::memset(palette, 0xFF, sizeof(palette));
    for (int i = 0; i < num_colors; ++i) {
        uint16_t idx = be16(data + c); c += 2;
        uint16_t r16 = be16(data + c); c += 2;
        uint16_t g16 = be16(data + c); c += 2;
        uint16_t b16 = be16(data + c); c += 2;
        uint8_t r = static_cast<uint8_t>(r16 >> 8);
        uint8_t g = static_cast<uint8_t>(g16 >> 8);
        uint8_t b = static_cast<uint8_t>(b16 >> 8);
        // Per QuickDraw, "value" is logical entry index 0..ctSize, but some
        // PICTs use it as the *target* palette slot. We honor both : use idx
        // when valid, else fall back to i.
        unsigned slot = (idx < 256) ? idx : static_cast<unsigned>(i);
        palette[slot][0] = r;
        palette[slot][1] = g;
        palette[slot][2] = b;
        palette[slot][3] = 0xFF;
    }

    if (is_region) {
        if (!need(2)) return false;
        uint16_t rsz = be16(data + c);
        if (rsz < 2 || !need(rsz)) return false;
        c += rsz;
    }
    if (!need(18)) return false;
    c += 8 + 8 + 2;  // srcRect + dstRect + mode

    int w = right - left;
    int h = bottom - top;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return false;
    if (pixelSize != 8) return false;  // only 8bpp indexed supported

    out->rgba.assign(static_cast<std::size_t>(w * h * 4), 0xFF);
    std::vector<uint8_t> row(static_cast<std::size_t>(w));

    if (packType == 1 /* unpacked */ || rowBytes < 8) {
        std::size_t bpr = static_cast<std::size_t>(rowBytes ? rowBytes : w);
        for (int y = 0; y < h; ++y) {
            if (!need(bpr)) return false;
            for (int x = 0; x < w; ++x) {
                uint8_t idx = data[c + x];
                uint8_t* px = out->rgba.data() + (y * w + x) * 4;
                px[0] = palette[idx][0];
                px[1] = palette[idx][1];
                px[2] = palette[idx][2];
                px[3] = 0xFF;
            }
            c += bpr;
        }
    } else {
        for (int y = 0; y < h; ++y) {
            std::size_t byteCount = 0;
            if (rowBytes < 250) {
                if (!need(1)) return false;
                byteCount = data[c]; c += 1;
            } else {
                if (!need(2)) return false;
                byteCount = be16(data + c); c += 2;
            }
            if (!need(byteCount)) return false;
            std::size_t got = unpack_bits(data + c, byteCount, row.data(), row.size());
            c += byteCount;
            if (got == 0) return false;
            for (int x = 0; x < w; ++x) {
                uint8_t idx = row[x];
                uint8_t* px = out->rgba.data() + (y * w + x) * 4;
                px[0] = palette[idx][0];
                px[1] = palette[idx][1];
                px[2] = palette[idx][2];
                px[3] = 0xFF;
            }
        }
    }
    out->width = w;
    out->height = h;
    return true;
}

/// Decode the DirectBitsRect opcode body, advancing the cursor.
/// Returns true if a bitmap was produced.
bool decode_direct_bits(const uint8_t* data, std::size_t size, std::size_t& c,
                        DecodedPict* out, bool is_region) {
    auto need = [&](std::size_t k) -> bool { return c + k <= size; };
    if (!need(46)) return false;

    // baseAddr u32 (only present for the non-region form... actually, for
    // DirectBitsRect (0x009A) it IS present per Inside Macintosh. Skip it.)
    c += 4;  // baseAddr
    uint16_t rowBytes = be16(data + c) & 0x3FFF; c += 2;
    int16_t  top    = be_i16(data + c); c += 2;
    int16_t  left   = be_i16(data + c); c += 2;
    int16_t  bottom = be_i16(data + c); c += 2;
    int16_t  right  = be_i16(data + c); c += 2;
    /*uint16_t pmVersion =*/ be16(data + c); c += 2;
    uint16_t packType = be16(data + c); c += 2;
    /*uint32_t packSize  =*/ be32(data + c); c += 4;
    c += 8;  // hRes + vRes
    /*uint16_t pixelType =*/ be16(data + c); c += 2;
    uint16_t pixelSize = be16(data + c); c += 2;
    uint16_t cmpCount  = be16(data + c); c += 2;
    uint16_t cmpSize   = be16(data + c); c += 2;
    c += 4 + 4 + 4;  // planeBytes + pmTable + pmReserved

    if (is_region) {
        // Skip maskRgn (variable)
        if (!need(2)) return false;
        uint16_t rsz = be16(data + c);
        if (rsz < 2 || !need(rsz)) return false;
        c += rsz;
    }
    if (!need(18)) return false;
    c += 8;  // srcRect (4 x int16)
    c += 8;  // dstRect
    c += 2;  // mode

    int w = right - left;
    int h = bottom - top;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return false;

    // Per-row decompressed size depends on the pack mode.
    // packType=3 (16-bit RGB555 packed per row): 2 bytes per pixel = 2*w
    // packType=4 (stripped components, cmpCount=3 RGB, 4 ARGB): cmpCount*w
    // unpacked (packType=1): rowBytes
    std::size_t row_size = 0;
    if (packType == 1) {
        row_size = static_cast<std::size_t>(rowBytes ? rowBytes : (w * (pixelSize / 8)));
    } else if (packType == 3) {
        row_size = static_cast<std::size_t>(w * 2);
    } else if (packType == 4) {
        row_size = static_cast<std::size_t>(w * (cmpCount ? cmpCount : 3));
    } else {
        row_size = static_cast<std::size_t>(rowBytes);
    }
    std::vector<uint8_t> row(row_size);
    out->rgba.assign(static_cast<std::size_t>(w * h * 4), 0xFF);

    auto write_pixel_rgb555 = [&](int x, int y, uint16_t v) {
        uint8_t r = static_cast<uint8_t>(((v >> 10) & 0x1F) << 3);
        uint8_t gn = static_cast<uint8_t>(((v >> 5) & 0x1F) << 3);
        uint8_t b = static_cast<uint8_t>(((v) & 0x1F) << 3);
        uint8_t* px = out->rgba.data() + (y * w + x) * 4;
        px[0] = r; px[1] = gn; px[2] = b; px[3] = 0xFF;
    };

    if (packType == 1 /* unpacked */) {
        // raw rows of pixelSize bits each
        std::size_t bytes_per_row = static_cast<std::size_t>(w) * (pixelSize / 8);
        for (int y = 0; y < h; ++y) {
            if (!need(bytes_per_row)) return false;
            if (pixelSize == 16) {
                for (int x = 0; x < w; ++x) {
                    write_pixel_rgb555(x, y, be16(data + c + x * 2));
                }
            } else if (pixelSize == 32) {
                for (int x = 0; x < w; ++x) {
                    const uint8_t* p = data + c + x * 4;
                    uint8_t* px = out->rgba.data() + (y * w + x) * 4;
                    // RGB direct : alpha, R, G, B
                    px[0] = p[1]; px[1] = p[2]; px[2] = p[3]; px[3] = 0xFF;
                }
            } else { return false; }
            c += bytes_per_row;
        }
        return true;
    }
    if (packType != 3 && packType != 4) return false;

    for (int y = 0; y < h; ++y) {
        // byteCount : u8 if rowBytes < 250, else u16
        std::size_t byteCount = 0;
        if (rowBytes < 250) {
            if (!need(1)) return false;
            byteCount = data[c]; c += 1;
        } else {
            if (!need(2)) return false;
            byteCount = be16(data + c); c += 2;
        }
        if (!need(byteCount)) return false;
        std::size_t got = unpack_bits(data + c, byteCount, row.data(), row.size());
        c += byteCount;
        if (got == 0) return false;

        if (packType == 3 && pixelSize == 16) {
            for (int x = 0; x < w; ++x) {
                uint16_t v = (row[x*2] << 8) | row[x*2 + 1];
                write_pixel_rgb555(x, y, v);
            }
        } else if (packType == 4 && pixelSize == 32 && cmpCount == 3 && cmpSize == 8) {
            // Stripped : R0..R(w-1) G0..G(w-1) B0..B(w-1)
            const uint8_t* R = row.data();
            const uint8_t* G = R + w;
            const uint8_t* B = G + w;
            for (int x = 0; x < w; ++x) {
                uint8_t* px = out->rgba.data() + (y * w + x) * 4;
                px[0] = R[x]; px[1] = G[x]; px[2] = B[x]; px[3] = 0xFF;
            }
        } else if (packType == 4 && pixelSize == 32 && cmpCount == 4 && cmpSize == 8) {
            // Stripped : A R G B planes
            const uint8_t* A = row.data();
            const uint8_t* R = A + w;
            const uint8_t* G = R + w;
            const uint8_t* B = G + w;
            for (int x = 0; x < w; ++x) {
                uint8_t* px = out->rgba.data() + (y * w + x) * 4;
                px[0] = R[x]; px[1] = G[x]; px[2] = B[x]; px[3] = A[x];
            }
        } else {
            return false;
        }
    }
    out->width = w;
    out->height = h;
    return true;
}

bool decode_jpeg(const uint8_t* jpeg, std::size_t jpeg_size, DecodedPict* out) {
    int w = 0, h = 0, comp = 0;
    unsigned char* px = stbi_load_from_memory(
        jpeg, static_cast<int>(jpeg_size), &w, &h, &comp, 4);
    if (!px) return false;
    out->width = w;
    out->height = h;
    out->rgba.assign(px, px + static_cast<std::size_t>(w * h * 4));
    stbi_image_free(px);
    return true;
}

}  // namespace

DecodedPict decode_pict(const uint8_t* data, std::size_t size) {
    DecodedPict out;
    if (!data || size < 10) {
        out.result = PictResult::EMPTY;
        return out;
    }
    // Header : u16 size + 4 x int16 rect
    /*uint16_t pict_size = be16(data + 0);*/  // not used
    int16_t top = be_i16(data + 2);
    int16_t left = be_i16(data + 4);
    int16_t bottom = be_i16(data + 6);
    int16_t right = be_i16(data + 8);
    out.width = right - left;
    out.height = bottom - top;

    std::size_t c = 10;
    auto need = [&](std::size_t k) -> bool { return c + k <= size; };

    while (need(2)) {
        uint16_t op = be16(data + c);
        c += 2;
        switch (op) {
        case 0x0000: break;                                // NOP
        case 0x0011: if (need(1)) c += 1; break;           // VersionOp
        case 0x001E: break;                                // DefHilite
        case 0x001F: if (need(6)) c += 6; break;           // OpColor
        case 0x0C00: if (need(24)) c += 24; break;         // HeaderOp v2
        case 0x02FF: if (need(2)) c += 2; break;           // Version2
        case 0x0007: if (need(4)) c += 4; break;           // PnSize
        case 0x0008: if (need(2)) c += 2; break;           // PnMode
        case 0x0009: if (need(8)) c += 8; break;           // PnPat
        case 0x000A: if (need(8)) c += 8; break;           // FillPat
        case 0x0001: {                                     // Clip
            if (!need(2)) { out.result = PictResult::TRUNCATED; return out; }
            uint16_t rsz = be16(data + c);
            if (rsz < 2 || !need(rsz)) { out.result = PictResult::TRUNCATED; return out; }
            c += rsz;
            break;
        }
        case 0x00A1: {                                     // LongComment
            if (!need(4)) { out.result = PictResult::TRUNCATED; return out; }
            uint16_t sz = be16(data + c + 2);
            if (!need(4 + sz)) { out.result = PictResult::TRUNCATED; return out; }
            c += 4 + sz;
            break;
        }
        case 0x8200: {                                     // CompressedQuickTime (JPEG)
            out.bitmap_opcode = op;
            if (!need(4)) { out.result = PictResult::TRUNCATED; return out; }
            uint32_t chunk_size = be32(data + c);
            if (chunk_size < 4 || !need(chunk_size)) { out.result = PictResult::TRUNCATED; return out; }
            // Scan for JPEG SOI within chunk
            std::size_t soi = find_jpeg_soi(data + c, chunk_size);
            if (soi >= chunk_size) {
                out.result = PictResult::JPEG_DECODE_FAILED;
                return out;
            }
            if (decode_jpeg(data + c + soi, chunk_size - soi, &out)) {
                out.result = PictResult::OK;
            } else {
                out.result = PictResult::JPEG_DECODE_FAILED;
            }
            return out;
        }
        case 0x8201: {                                     // UncompressedQuickTime
            out.bitmap_opcode = op;
            if (!need(4)) { out.result = PictResult::TRUNCATED; return out; }
            uint32_t sz = be32(data + c);
            if (!need(4 + sz)) { out.result = PictResult::TRUNCATED; return out; }
            out.result = PictResult::UNSUPPORTED_BITMAP;
            return out;
        }
        case 0x009A:                                       // DirectBitsRect
        case 0x009B:                                       // DirectBitsRgn
            out.bitmap_opcode = op;
            if (decode_direct_bits(data, size, c, &out, op == 0x009B)) {
                out.result = PictResult::OK;
            } else {
                out.result = PictResult::UNSUPPORTED_BITMAP;
            }
            return out;
        case 0x0098:                                       // PackBitsRect
        case 0x0099:                                       // PackBitsRgn
            out.bitmap_opcode = op;
            if (decode_packbits_rect(data, size, c, &out, op == 0x0099)) {
                out.result = PictResult::OK;
            } else {
                out.result = PictResult::UNSUPPORTED_BITMAP;
            }
            return out;
        case 0x00FF:                                       // OpEndPic
            if (out.bitmap_opcode == 0) out.result = PictResult::NO_BITMAP;
            return out;
        default:
            // Unknown opcode — bail to avoid mis-skipping
            out.result = PictResult::UNSUPPORTED_BITMAP;
            out.bitmap_opcode = op;
            return out;
        }
        // PICT v2 word-alignment
        if (c & 1u) ++c;
    }
    out.result = (out.bitmap_opcode == 0) ? PictResult::NO_BITMAP : PictResult::TRUNCATED;
    return out;
}

DecodedPict decode_jpeg_rgba(const uint8_t* data, std::size_t size) {
    DecodedPict out;
    if (!data || size < 4) {
        out.result = PictResult::EMPTY;
        return out;
    }
    if (decode_jpeg(data, size, &out)) {
        out.result = PictResult::OK;
        out.bitmap_opcode = 0x8200;  // synthetic, signals "embedded JPEG"
        return out;
    }
    out.result = PictResult::JPEG_DECODE_FAILED;
    return out;
}

}  // namespace assets
}  // namespace xfiles
