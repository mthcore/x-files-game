// SPDX-License-Identifier: MIT
#include "runtime/fmv_player.h"

#include <SDL.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "assets/ima_adpcm.h"

namespace xfiles::runtime {

namespace {
constexpr uint32_t kCvid = xfiles::assets::qt_fourcc('c', 'v', 'i', 'd');
constexpr uint32_t kIma4 = xfiles::assets::qt_fourcc('i', 'm', 'a', '4');
}  // namespace

FmvPlayer::FmvPlayer() = default;

FmvPlayer::~FmvPlayer() {
    if (texture_) SDL_DestroyTexture(texture_);
}

bool FmvPlayer::open(const std::string& path) {
    done_ = false;
    frame_index_ = 0;
    last_rgba_.clear();
    pcm_.clear();
    if (!xfiles::assets::qt_load(path, &source_)) {
        std::fprintf(stderr, "fmv: qt_load failed for %s\n", path.c_str());
        done_ = true;
        return false;
    }
    video_ = source_.video;
    audio_track_ = source_.audio;
    if (video_.fourcc != kCvid) {
        std::fprintf(stderr, "fmv: %s video codec is not Cinepak (0x%08x)\n",
                     path.c_str(), video_.fourcc);
        done_ = true;
        return false;
    }
    frame_rate_ = video_.frame_rate();
    if (frame_rate_ <= 0.0) frame_rate_ = 15.0;
    if (video_.frame_count == 0) {
        done_ = true;
        return false;
    }
    return true;
}

bool FmvPlayer::attach(SDL_Renderer* ren, SDL_AudioDeviceID audio) {
    if (!ren || video_.frame_count == 0) return false;
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    tex_w_ = video_.width;
    tex_h_ = video_.height;
    texture_ = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  tex_w_, tex_h_);
    if (!texture_) {
        std::fprintf(stderr, "fmv: SDL_CreateTexture failed: %s\n",
                     SDL_GetError());
        return false;
    }
    // Pre-decode the audio track if it is IMA4. Anything else stays silent.
    if (audio && audio_track_.fourcc == kIma4 &&
        !audio_track_.sample_offsets.empty()) {
        // Pull every IMA packet into one contiguous buffer first.
        std::size_t total_packets = audio_track_.sample_offsets.size();
        std::vector<uint8_t> packets(total_packets *
                                       xfiles::assets::IMA_PACKET_BYTES);
        for (std::size_t i = 0; i < total_packets; ++i) {
            const uint8_t* p = source_.data.data() +
                                audio_track_.sample_offsets[i];
            std::memcpy(packets.data() +
                            i * xfiles::assets::IMA_PACKET_BYTES,
                         p, xfiles::assets::IMA_PACKET_BYTES);
        }
        if (audio_track_.channels == 2 && (total_packets % 2) == 0) {
            pcm_.resize(total_packets / 2 *
                          xfiles::assets::IMA_SAMPLES_PER_PACKET * 2);
            xfiles::assets::ima_decode_stereo_stream(
                packets.data(), total_packets, pcm_.data());
        } else {
            std::vector<int16_t> mono(total_packets *
                                        xfiles::assets::IMA_SAMPLES_PER_PACKET);
            xfiles::assets::ima_decode_mono_stream(
                packets.data(), total_packets, mono.data());
            // Mirror mono into stereo so SDL audio device shape stays uniform.
            pcm_.resize(mono.size() * 2);
            for (std::size_t i = 0; i < mono.size(); ++i) {
                pcm_[i * 2 + 0] = mono[i];
                pcm_[i * 2 + 1] = mono[i];
            }
        }
        const uint32_t bytes = static_cast<uint32_t>(pcm_.size() *
                                                        sizeof(int16_t));
        if (bytes && SDL_QueueAudio(audio, pcm_.data(), bytes) != 0) {
            std::fprintf(stderr, "fmv: SDL_QueueAudio failed: %s\n",
                         SDL_GetError());
            // Non-fatal — we still play the video silently.
        }
    }
    start_time_sec_ = static_cast<double>(SDL_GetTicks64()) / 1000.0;
    return load_frame_(0);
}

bool FmvPlayer::load_frame_(std::size_t i) {
    if (i >= video_.sample_offsets.size()) {
        done_ = true;
        return false;
    }
    std::size_t size = 0;
    const uint8_t* bytes = xfiles::assets::qt_video_sample(source_, i, &size);
    if (!bytes || size == 0) {
        done_ = true;
        return false;
    }
    auto frame = decoder_.decode_frame(bytes, size);
    if (!frame.ok || frame.rgba.empty()) {
        done_ = true;
        return false;
    }
    last_rgba_ = std::move(frame.rgba);
    if (texture_) {
        SDL_UpdateTexture(texture_, nullptr, last_rgba_.data(),
                           video_.width * 4);
    }
    frame_index_ = i;
    return true;
}

bool FmvPlayer::pump() {
    if (done_ || !texture_) return false;
    double elapsed = static_cast<double>(SDL_GetTicks64()) / 1000.0 -
                      start_time_sec_;
    std::size_t target = static_cast<std::size_t>(elapsed * frame_rate_);
    if (target <= frame_index_) return false;
    if (target >= video_.sample_offsets.size()) {
        target = video_.sample_offsets.size() - 1;
        done_ = true;
    }
    bool drew = false;
    while (frame_index_ < target && !done_) {
        ++frame_index_;
        drew = load_frame_(frame_index_) || drew;
    }
    return drew;
}

void FmvPlayer::render(SDL_Renderer* ren) {
    if (!ren || !texture_) return;
    SDL_RenderCopy(ren, texture_, nullptr, nullptr);
}

}  // namespace xfiles::runtime
