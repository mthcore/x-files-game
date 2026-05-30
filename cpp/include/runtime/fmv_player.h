// SPDX-License-Identifier: MIT
//
// Cinepak + IMA ADPCM playback wrapper over the byte-direct codec library
// already in `cpp/include/assets/`. Pumps one Cinepak frame at the source's
// declared rate into an `SDL_Texture` and pre-queues the IMA-decoded stereo
// audio (or mono, mirrored to stereo) into an `SDL_AudioDeviceID`.
//
// This is a small playable demonstrator, not a frame-accurate AV sync engine.
// Slice 1 keeps it simple: load the whole file, decode all video frames once
// into a frame ring, decode all audio packets once into a PCM buffer, then
// step through frames in real time. A real game would stream both.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;
typedef uint32_t SDL_AudioDeviceID;

#include "assets/cinepak_decoder.h"
#include "assets/qt_container.h"

namespace xfiles::runtime {

class FmvPlayer {
public:
    FmvPlayer();
    ~FmvPlayer();
    FmvPlayer(const FmvPlayer&) = delete;
    FmvPlayer& operator=(const FmvPlayer&) = delete;

    // Open a QuickTime/Cinepak/IMA file. Returns false if the file cannot be
    // read, the QuickTime container fails to parse, or the video codec is
    // not Cinepak. On false, the player is left empty and `done()` is true.
    bool open(const std::string& path);

    // Allocate (or re-allocate) the SDL texture and queue the audio buffer.
    // Must be called once after `open` and after the renderer / audio device
    // are ready. Returns false on SDL failures (texture creation, audio queue).
    bool attach(SDL_Renderer* ren, SDL_AudioDeviceID audio);

    // Advance to the next frame whose presentation time has elapsed (based on
    // the file's declared frame rate and an internal monotonic counter). Does
    // not block; returns true if a new frame was uploaded to the texture.
    bool pump();

    // Copy the current frame onto the active renderer (filling the window).
    void render(SDL_Renderer* ren);

    // True once every frame in the source has been presented at least once.
    bool done() const { return done_; }

    int width()  const { return video_.width; }
    int height() const { return video_.height; }

private:
    xfiles::assets::QtFile          source_;
    xfiles::assets::CinepakDecoder  decoder_;
    xfiles::assets::QtVideoTrack    video_{};
    xfiles::assets::QtAudioTrack    audio_track_{};

    SDL_Texture* texture_ = nullptr;
    int          tex_w_   = 0;
    int          tex_h_   = 0;

    // Frame timing.
    double  frame_rate_      = 15.0;
    double  start_time_sec_  = 0.0;
    std::size_t frame_index_ = 0;
    bool        done_        = false;

    // Decoded audio (interleaved stereo S16LE at the source's sample rate).
    std::vector<int16_t> pcm_;

    bool load_frame_(std::size_t i);
    std::vector<uint8_t> last_rgba_;
};

}  // namespace xfiles::runtime
