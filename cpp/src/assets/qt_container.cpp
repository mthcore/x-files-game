// SPDX-License-Identifier: MIT
// QuickTime container parser implementation.
//
// We walk only the atoms required to locate video sample bytes :
//   moov / trak / mdia / minf / stbl / stsd  -> codec fourcc + width/height
//                                  / stco    -> chunk offsets (in mdat)
//                                  / stsc    -> chunks-per-sample mapping
//                                  / stsz    -> sample sizes
//                                  / stts    -> time-to-sample (frame count)
// Other atoms (udta, free, skip, ...) are skipped.

#include "assets/qt_container.h"

#include <cstdio>
#include <cstring>

namespace xfiles {
namespace assets {

namespace {

inline uint32_t be32(const uint8_t* p) {
    return  (uint32_t(p[0]) << 24)
         |  (uint32_t(p[1]) << 16)
         |  (uint32_t(p[2]) <<  8)
         |   uint32_t(p[3]);
}
inline uint16_t be16(const uint8_t* p) {
    return uint16_t((uint16_t(p[0]) << 8) | uint16_t(p[1]));
}

struct StscEntry { uint32_t first_chunk; uint32_t samples_per_chunk; uint32_t desc_idx; };

struct ParseState {
    QtVideoTrack video;            ///< Final committed video track
    QtAudioTrack audio;            ///< Final committed audio track
    // Per-TRAK scratch (cleared when entering a TRAK, committed on close).
    enum TrakKind { TRAK_OTHER, TRAK_VIDEO, TRAK_AUDIO } trak_kind = TRAK_OTHER;
    bool got_stsd = false;
    uint32_t cur_fourcc = 0;
    uint16_t cur_width = 0;
    uint16_t cur_height = 0;
    uint16_t cur_channels = 0;
    uint16_t cur_sample_size = 0;
    uint32_t cur_sample_rate = 0;
    std::vector<uint32_t> chunk_offsets;
    std::vector<uint32_t> sample_sizes;
    uint32_t default_sample_size = 0;
    std::vector<StscEntry> stsc;
    uint32_t total_frames_stts = 0;
    uint32_t total_duration_stts = 0;
    uint32_t mdia_timescale = 0;
    /// elst first-entry mediaTime, captured per trak.
    int32_t  edit_media_time = 0;
};

// Walk container atoms recursively. Each atom: u32 size + 4-byte type + payload.
// size == 1 means extended 64-bit size follows (we don't expect that in the on-disk format).
bool walk(const uint8_t* p, std::size_t n, ParseState* st, int depth);

bool parse_stsd(const uint8_t* p, std::size_t n, ParseState* st) {
    if (n < 8) return false;
    p += 4 + 4;  // version + entries count
    n -= 8;
    if (n < 8) return false;
    // First sample description : u32 size + u32 fourcc + ...
    /*uint32_t desc_size =*/ be32(p);
    uint32_t fourcc = be32(p + 4);
    st->cur_fourcc = fourcc;
    if (st->trak_kind == ParseState::TRAK_VIDEO) {
        // VisualSampleEntry : w/h at +32..+35.
        if (n >= 36) {
            st->cur_width  = be16(p + 32);
            st->cur_height = be16(p + 34);
        }
    } else if (st->trak_kind == ParseState::TRAK_AUDIO) {
        // AudioSampleEntry layout (QT v0) from sample-description start :
        //   +0..3   size      +4..7   fourcc
        //   +8..13  6 reserved bytes
        //   +14..15 dref_idx
        //   +16..17 version  +18..19 revision  +20..23 vendor
        //   +24..25 channels +26..27 sample_size
        //   +28..29 compression_id +30..31 packet_size
        //   +32..33 sample_rate integer +34..35 sample_rate frac
        if (n >= 36) {
            st->cur_channels    = be16(p + 24);
            st->cur_sample_size = be16(p + 26);
            st->cur_sample_rate = static_cast<uint32_t>(be16(p + 32));  // integer part of 16.16
        }
    }
    st->got_stsd = true;
    return true;
}

bool parse_stco(const uint8_t* p, std::size_t n, ParseState* st) {
    if (n < 8) return false;
    uint32_t cnt = be32(p + 4);
    if (n < 8u + cnt * 4u) return false;
    st->chunk_offsets.assign(cnt, 0);
    for (uint32_t i = 0; i < cnt; ++i) st->chunk_offsets[i] = be32(p + 8 + i * 4);
    return true;
}

bool parse_stsz(const uint8_t* p, std::size_t n, ParseState* st) {
    if (n < 12) return false;
    st->default_sample_size = be32(p + 4);
    uint32_t cnt = be32(p + 8);
    if (st->default_sample_size == 0) {
        if (n < 12u + cnt * 4u) return false;
        st->sample_sizes.assign(cnt, 0);
        for (uint32_t i = 0; i < cnt; ++i) st->sample_sizes[i] = be32(p + 12 + i * 4);
    } else {
        st->sample_sizes.assign(cnt, st->default_sample_size);
    }
    return true;
}

bool parse_stsc(const uint8_t* p, std::size_t n, ParseState* st) {
    if (n < 8) return false;
    uint32_t cnt = be32(p + 4);
    if (n < 8u + cnt * 12u) return false;
    st->stsc.assign(cnt, {0, 0, 0});
    for (uint32_t i = 0; i < cnt; ++i) {
        st->stsc[i].first_chunk       = be32(p + 8 + i * 12);
        st->stsc[i].samples_per_chunk = be32(p + 12 + i * 12);
        st->stsc[i].desc_idx          = be32(p + 16 + i * 12);
    }
    return true;
}

bool parse_stts(const uint8_t* p, std::size_t n, ParseState* st) {
    if (n < 8) return false;
    uint32_t cnt = be32(p + 4);
    if (n < 8u + cnt * 8u) return false;
    uint64_t total_frames = 0;
    uint64_t total_dur    = 0;
    for (uint32_t i = 0; i < cnt; ++i) {
        uint32_t sample_count    = be32(p + 8 + i * 8);
        uint32_t sample_duration = be32(p + 12 + i * 8);
        total_frames += sample_count;
        total_dur    += static_cast<uint64_t>(sample_count) * sample_duration;
    }
    st->total_frames_stts  = static_cast<uint32_t>(total_frames);
    st->total_duration_stts = static_cast<uint32_t>(total_dur);
    return true;
}

bool parse_mdhd(const uint8_t* p, std::size_t n, ParseState* st) {
    // version (1) + flags (3), then v0: cT(4) + mT(4) + ts(4) + dur(4)
    //                          v1: cT(8) + mT(8) + ts(4) + dur(8)
    if (n < 24) return false;
    uint8_t version = p[0];
    if (version == 1) {
        if (n < 36) return false;
        st->mdia_timescale = be32(p + 4 + 16);
    } else {
        st->mdia_timescale = be32(p + 4 + 8);
    }
    return true;
}

bool parse_elst(const uint8_t* p, std::size_t n, ParseState* st) {
    // version(1) + flags(3) + entry_count(4) + entries(12 each v0 or 20 v1)
    if (n < 8) return false;
    uint8_t version = p[0];
    uint32_t cnt = be32(p + 4);
    if (cnt == 0) return true;
    // Read FIRST entry's mediaTime. XMVs all use a single elst entry.
    if (version == 0) {
        if (n < 8u + 12u) return false;
        // duration(4) + media_time(4 signed) + media_rate(4)
        uint32_t mt_u = be32(p + 8 + 4);
        st->edit_media_time = static_cast<int32_t>(mt_u);
    } else {
        if (n < 8u + 20u) return false;
        // duration(8) + media_time(8 signed) + media_rate(4)
        uint32_t mt_hi = be32(p + 8 + 8);
        uint32_t mt_lo = be32(p + 8 + 12);
        // We only support fitting into int32 (XMVs : mediaTime=48)
        if (mt_hi == 0) st->edit_media_time = static_cast<int32_t>(mt_lo);
    }
    return true;
}

bool parse_hdlr(const uint8_t* p, std::size_t n, ParseState* st) {
    if (n < 12) return false;
    uint32_t qt_type = be32(p + 4);
    uint32_t subtype = be32(p + 8);
    constexpr uint32_t mhlr_ = qt_fourcc('m','h','l','r');
    constexpr uint32_t vide_ = qt_fourcc('v','i','d','e');
    constexpr uint32_t soun_ = qt_fourcc('s','o','u','n');
    if (qt_type == mhlr_) {
        if (subtype == vide_)      st->trak_kind = ParseState::TRAK_VIDEO;
        else if (subtype == soun_) st->trak_kind = ParseState::TRAK_AUDIO;
        else                       st->trak_kind = ParseState::TRAK_OTHER;
    }
    return true;
}

bool walk(const uint8_t* p, std::size_t n, ParseState* st, int depth) {
    std::size_t c = 0;
    while (c + 8 <= n) {
        uint32_t sz = be32(p + c);
        uint32_t type = be32(p + c + 4);
        if (sz == 0) break;              // last atom : to end of file
        if (sz < 8 || c + sz > n) break;  // corrupt
        const uint8_t* body = p + c + 8;
        std::size_t   blen = sz - 8;

        char tag[5] = {0};
        tag[0] = (type >> 24) & 0xFF;
        tag[1] = (type >> 16) & 0xFF;
        tag[2] = (type >>  8) & 0xFF;
        tag[3] = (type)       & 0xFF;
        (void)tag;

        const uint32_t MOOV = qt_fourcc('m','o','o','v');
        const uint32_t TRAK = qt_fourcc('t','r','a','k');
        const uint32_t MDIA = qt_fourcc('m','d','i','a');
        const uint32_t MINF = qt_fourcc('m','i','n','f');
        const uint32_t STBL = qt_fourcc('s','t','b','l');
        const uint32_t HDLR = qt_fourcc('h','d','l','r');
        const uint32_t STSD = qt_fourcc('s','t','s','d');
        const uint32_t STCO = qt_fourcc('s','t','c','o');
        const uint32_t STSZ = qt_fourcc('s','t','s','z');
        const uint32_t STSC = qt_fourcc('s','t','s','c');
        const uint32_t STTS = qt_fourcc('s','t','t','s');

        const uint32_t EDTS = qt_fourcc('e','d','t','s');
        const uint32_t ELST = qt_fourcc('e','l','s','t');

        if (type == MOOV || type == TRAK || type == MDIA
            || type == MINF || type == STBL || type == EDTS) {
            // Reset video-track scope at the start of each TRAK
            if (type == TRAK) {
                st->trak_kind = ParseState::TRAK_OTHER;
                st->got_stsd = false;
                st->cur_fourcc = 0;
                st->cur_width = 0;
                st->cur_height = 0;
                st->cur_channels = 0;
                st->cur_sample_size = 0;
                st->cur_sample_rate = 0;
                st->chunk_offsets.clear();
                st->sample_sizes.clear();
                st->stsc.clear();
                st->total_frames_stts = 0;
                st->total_duration_stts = 0;
                st->mdia_timescale = 0;
                st->edit_media_time = 0;
            }
            walk(body, blen, st, depth + 1);

            // On TRAK close, if it was the video track, snapshot extracted data.
            if (type == TRAK && st->trak_kind == ParseState::TRAK_VIDEO && st->got_stsd) {
                st->video.fourcc = st->cur_fourcc;
                st->video.width = st->cur_width;
                st->video.height = st->cur_height;
                st->video.frame_count = st->total_frames_stts;
                st->video.timescale = st->mdia_timescale;
                st->video.total_duration = st->total_duration_stts;
                // Build per-frame offset list from chunk_offsets + stsc + sample_sizes.
                // Many XMV files keep 1 frame per chunk : simple direct mapping.
                std::vector<uint32_t> offsets;
                std::vector<uint32_t> sizes;
                // Walk chunks and assign N samples each.
                if (!st->stsc.empty() && !st->chunk_offsets.empty()) {
                    std::size_t sample_cursor = 0;
                    for (std::size_t ci = 0; ci < st->chunk_offsets.size(); ++ci) {
                        // Find samples_per_chunk for this chunk.
                        uint32_t spc = st->stsc.back().samples_per_chunk;
                        for (std::size_t i = 0; i < st->stsc.size(); ++i) {
                            uint32_t first = st->stsc[i].first_chunk;
                            if (first - 1 <= ci) spc = st->stsc[i].samples_per_chunk;
                            if (i + 1 < st->stsc.size()
                                && (st->stsc[i + 1].first_chunk - 1) > ci) break;
                        }
                        uint32_t cur = st->chunk_offsets[ci];
                        for (uint32_t s = 0; s < spc; ++s) {
                            if (sample_cursor >= st->sample_sizes.size()) break;
                            uint32_t ssz = st->sample_sizes[sample_cursor++];
                            offsets.push_back(cur);
                            sizes.push_back(ssz);
                            cur += ssz;
                        }
                    }
                }
                st->video.sample_offsets = std::move(offsets);
                st->video.sample_sizes   = std::move(sizes);
            }
            // On TRAK close, if it was audio, snapshot it too.
            if (type == TRAK && st->trak_kind == ParseState::TRAK_AUDIO && st->got_stsd) {
                st->audio.fourcc       = st->cur_fourcc;
                st->audio.channels     = st->cur_channels;
                st->audio.sample_size  = st->cur_sample_size;
                st->audio.sample_rate  = st->cur_sample_rate;
                st->audio.sample_count = st->total_frames_stts;
                st->audio.timescale    = st->mdia_timescale;
                // mediaTime from elst is in mdia timescale units. For IMA4 the
                // timescale equals sample_rate so this is a PCM-sample count.
                st->audio.edit_skip_samples =
                    (st->edit_media_time > 0) ? static_cast<uint32_t>(st->edit_media_time) : 0u;
                std::vector<uint32_t> offsets;
                std::vector<uint32_t> sizes;
                constexpr uint32_t IMA4 = qt_fourcc('i','m','a','4');
                if (st->cur_fourcc == IMA4 && st->cur_channels > 0
                    && !st->stsc.empty() && !st->chunk_offsets.empty()) {
                    // For IMA4 the stsz/stsc figures are denominated in PCM
                    // samples per channel, not in bytes. Each frame stores
                    // `channels` packets of 34 bytes, interleaved L,R,L,R per
                    // frame. Walk chunks and emit one 34-byte sample_offsets
                    // entry per channel per frame.
                    constexpr uint32_t BYTES_PER_PACKET   = 34;
                    constexpr uint32_t SAMPLES_PER_PACKET = 64;
                    uint32_t ch = st->cur_channels;
                    for (std::size_t ci = 0; ci < st->chunk_offsets.size(); ++ci) {
                        uint32_t spc = st->stsc.back().samples_per_chunk;
                        for (std::size_t i = 0; i < st->stsc.size(); ++i) {
                            uint32_t first = st->stsc[i].first_chunk;
                            if (first - 1 <= ci) spc = st->stsc[i].samples_per_chunk;
                            if (i + 1 < st->stsc.size()
                                && (st->stsc[i + 1].first_chunk - 1) > ci) break;
                        }
                        uint32_t frames_in_chunk = spc / SAMPLES_PER_PACKET;
                        uint32_t cur = st->chunk_offsets[ci];
                        for (uint32_t f = 0; f < frames_in_chunk; ++f) {
                            for (uint32_t c2 = 0; c2 < ch; ++c2) {
                                offsets.push_back(cur);
                                sizes.push_back(BYTES_PER_PACKET);
                                cur += BYTES_PER_PACKET;
                            }
                        }
                    }
                } else if (!st->stsc.empty() && !st->chunk_offsets.empty()) {
                    // Generic path (PCM, etc.) : sample_sizes drive bytes per sample.
                    std::size_t sample_cursor = 0;
                    for (std::size_t ci = 0; ci < st->chunk_offsets.size(); ++ci) {
                        uint32_t spc = st->stsc.back().samples_per_chunk;
                        for (std::size_t i = 0; i < st->stsc.size(); ++i) {
                            uint32_t first = st->stsc[i].first_chunk;
                            if (first - 1 <= ci) spc = st->stsc[i].samples_per_chunk;
                            if (i + 1 < st->stsc.size()
                                && (st->stsc[i + 1].first_chunk - 1) > ci) break;
                        }
                        uint32_t cur = st->chunk_offsets[ci];
                        for (uint32_t s = 0; s < spc; ++s) {
                            if (sample_cursor >= st->sample_sizes.size()) break;
                            uint32_t ssz = st->sample_sizes[sample_cursor++];
                            offsets.push_back(cur);
                            sizes.push_back(ssz);
                            cur += ssz;
                        }
                    }
                }
                st->audio.sample_offsets = std::move(offsets);
                st->audio.sample_sizes   = std::move(sizes);
            }
        } else if (type == ELST) {
            parse_elst(body, blen, st);
        } else if (type == HDLR) {
            parse_hdlr(body, blen, st);
        } else if (type == STSD) {
            parse_stsd(body, blen, st);
        } else if (type == STCO) {
            parse_stco(body, blen, st);
        } else if (type == STSZ) {
            parse_stsz(body, blen, st);
        } else if (type == STSC) {
            parse_stsc(body, blen, st);
        } else if (type == STTS) {
            parse_stts(body, blen, st);
        } else if (type == qt_fourcc('m','d','h','d')) {
            parse_mdhd(body, blen, st);
        }
        // else : ignore (mdat / udta / free / skip / etc.)

        c += sz;
    }
    return true;
}

}  // namespace

bool qt_load(const std::string& path, QtFile* out) {
    if (!out) return false;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    if (sz < 16) { std::fclose(f); return false; }
    std::fseek(f, 0, SEEK_SET);
    out->data.resize(static_cast<std::size_t>(sz));
    std::size_t n = std::fread(out->data.data(), 1, out->data.size(), f);
    std::fclose(f);
    if (n != out->data.size()) return false;

    ParseState st;
    walk(out->data.data(), out->data.size(), &st, 0);
    out->video = std::move(st.video);
    out->audio = std::move(st.audio);
    //  accept audio-only containers (.AMV companion streams)
    // — fourcc/frame_count are 0 for those, but audio.fourcc + sample
    // tables are populated.
    bool has_video = (out->video.fourcc != 0 && out->video.frame_count > 0);
    bool has_audio = (out->audio.fourcc != 0 && !out->audio.sample_offsets.empty());
    return has_video || has_audio;
}

const uint8_t* qt_video_sample(const QtFile& f, std::size_t i, std::size_t* out_size) {
    if (out_size) *out_size = 0;
    if (i >= f.video.sample_offsets.size()) return nullptr;
    uint32_t off = f.video.sample_offsets[i];
    uint32_t sz  = f.video.sample_sizes[i];
    if (off + sz > f.data.size()) return nullptr;
    if (out_size) *out_size = sz;
    return f.data.data() + off;
}

}  // namespace assets
}  // namespace xfiles
