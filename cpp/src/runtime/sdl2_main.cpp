// SPDX-License-Identifier: MIT
//
// xfiles_play — minimal SDL2 playable shell.
//
// Slice 1: open a window, play one Cinepak/IMA scene from `XV/<scene_id>.xmv`,
// overlay its HSPT hotspot rects, dispatch the rect's action_id on click. The
// goal is to demonstrate the full byte-direct chain end to end in 640x480 —
// the data layer is reused as-is, only the presentation glue is new.
//
// Flags:
//   --asset-dir <path>     directory containing XV/<scene_id>.{xmv,HOT}
//   --scene <id>           starting scene id (default: 19672 = Field Office)
//   --debug-hotspots       draw rect outlines (toggle at runtime with F1)
//   --probe                load every artifact, print state, exit 0 BEFORE
//                          SDL_Init — safe for headless CI.
//
// Keys at runtime: ESC quit, F1 toggle overlay, SPACE replay current scene.
#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "runtime/fmv_player.h"
#include "runtime/hsp_loader.h"
#include "runtime/scene_resolver.h"

namespace {

constexpr int kWindowW = 640;
constexpr int kWindowH = 480;
constexpr uint32_t kDefaultScene = 19672;   // Field Office, demo'd by xfiles_engine

struct Args {
    std::string asset_dir;
    uint32_t scene_id = kDefaultScene;
    bool debug_hotspots = false;
    bool probe = false;
};

void print_usage() {
    std::printf(
        "Usage: xfiles_play [--asset-dir <dir>] [--scene <id>]\n"
        "                   [--debug-hotspots] [--probe]\n"
        "\n"
        "  --asset-dir <dir>   directory holding XV/<scene_id>.{xmv,HOT}\n"
        "                      (defaults to ./XV)\n"
        "  --scene <id>        scene id to load (default: %u)\n"
        "  --debug-hotspots    draw hotspot rect outlines (toggle: F1)\n"
        "  --probe             load + report + exit 0, no SDL init\n",
        kDefaultScene);
}

bool parse(int argc, char** argv, Args& out) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--asset-dir" && i + 1 < argc) { out.asset_dir = argv[++i]; }
        else if (a == "--scene" && i + 1 < argc) {
            out.scene_id = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        }
        else if (a == "--debug-hotspots") { out.debug_hotspots = true; }
        else if (a == "--probe") { out.probe = true; }
        else if (a == "--help" || a == "-h") { print_usage(); return false; }
        else { std::fprintf(stderr, "unknown arg: %s\n", a.c_str()); print_usage(); return false; }
    }
    if (out.asset_dir.empty()) out.asset_dir = "XV";
    return true;
}

std::string join_path(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    char last = dir.back();
    if (last == '/' || last == '\\') return dir + name;
    return dir + "/" + name;
}

}  // namespace

int main(int argc, char** argv) {
    Args args;
    if (!parse(argc, argv, args)) return 1;

    const std::string sid_str = std::to_string(args.scene_id);
    const std::string hot_path = join_path(args.asset_dir, sid_str + ".HOT");
    const std::string xmv_path = join_path(args.asset_dir, sid_str + ".xmv");

    // Load HSPT rects up front (cheap, no SDL needed).
    auto rects = xfiles::runtime::load_hot_file(hot_path);
    bool hot_present = std::filesystem::is_regular_file(hot_path);
    bool xmv_present = std::filesystem::is_regular_file(xmv_path);

    std::printf("xfiles_play: scene_id=%u  hot=%s  xmv=%s\n",
                args.scene_id,
                hot_present ? "found" : "missing",
                xmv_present ? "found" : "missing");
    std::printf("  hotspots loaded: %zu rects from %s\n", rects.size(),
                hot_present ? hot_path.c_str() : "(no .HOT file)");

    if (args.probe) {
        const bool ready = hot_present && xmv_present;
        std::printf("probe ok, scene_id=%u rects=%zu video=%s status=%s\n",
                     args.scene_id, rects.size(),
                     xmv_present ? "present" : "absent",
                     ready ? "ready" : "fallback");
        return 0;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 2;
    }

    SDL_Window* win = SDL_CreateWindow(
        "X-Files (decoder shell)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kWindowW, kWindowH, 0);
    if (!win) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 3;
    }
    SDL_Renderer* ren = SDL_CreateRenderer(
        win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 3;
    }

    SDL_AudioSpec want{};
    want.freq = 22050;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    SDL_AudioSpec got{};
    SDL_AudioDeviceID audio = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
    if (audio == 0) {
        std::fprintf(stderr, "SDL_OpenAudioDevice failed: %s (continuing silent)\n",
                     SDL_GetError());
    }

    xfiles::runtime::FmvPlayer fmv;
    bool fmv_ok = false;
    if (xmv_present) {
        fmv_ok = fmv.open(xmv_path);
        if (fmv_ok) fmv_ok = fmv.attach(ren, audio);
    }
    if (!fmv_ok) {
        std::printf("xfiles_play: no playable XV — falling back to checker pattern\n");
    }
    if (audio) SDL_PauseAudioDevice(audio, 0);

    bool debug = args.debug_hotspots;
    bool running = true;
    int clicks = 0;
    int last_action_id = 0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) running = false;
                if (e.key.keysym.sym == SDLK_F1) debug = !debug;
                if (e.key.keysym.sym == SDLK_SPACE && xmv_present) {
                    fmv.open(xmv_path);
                    fmv.attach(ren, audio);
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int idx = xfiles::runtime::hit_test(rects, e.button.x, e.button.y);
                if (idx >= 0) {
                    const auto& h = rects[idx];
                    last_action_id = static_cast<int>(h.action_id_1);
                    ++clicks;
                    std::printf("click (%d,%d) -> rect#%d "
                                 "[%d,%d]-[%d,%d] action_id_1=%u action_id_2=%u\n",
                                 e.button.x, e.button.y, idx,
                                 h.x_min, h.y_min, h.x_max, h.y_max,
                                 h.action_id_1, h.action_id_2);
                }
            }
        }

        // Pump video.
        if (fmv_ok) fmv.pump();

        // Render.
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        if (fmv_ok) {
            fmv.render(ren);
        } else {
            // Checker fallback: 32-px squares so the scene is visible.
            for (int y = 0; y < kWindowH; y += 32) {
                for (int x = 0; x < kWindowW; x += 32) {
                    bool dark = ((x / 32) + (y / 32)) & 1;
                    SDL_SetRenderDrawColor(ren, dark ? 30 : 50, dark ? 30 : 50,
                                            dark ? 60 : 80, 255);
                    SDL_Rect r{ x, y, 32, 32 };
                    SDL_RenderFillRect(ren, &r);
                }
            }
        }

        if (debug) {
            SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
            for (const auto& h : rects) {
                SDL_Rect r{ h.x_min, h.y_min,
                            h.x_max - h.x_min, h.y_max - h.y_min };
                SDL_RenderDrawRect(ren, &r);
            }
        }

        SDL_RenderPresent(ren);
    }

    std::printf("\nexit summary:\n");
    std::printf("  clicks         : %d\n", clicks);
    std::printf("  last action_id : %d\n", last_action_id);
    std::printf("  scene_id       : %u\n", args.scene_id);

    if (audio) SDL_CloseAudioDevice(audio);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
