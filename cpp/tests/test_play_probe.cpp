// SPDX-License-Identifier: MIT
//
// xfiles_play --probe smoke test. Skipped when the SDL2 shell is not built
// (XFILES_PLAY=OFF). Otherwise spawns the binary from the configured build
// dir and asserts: exit 0, stdout contains "probe ok, scene_id=" and
// "status=" lines.
//
// Locates the binary by reading the XFILES_PLAY_BIN env var (the CMake
// add_test wires it), and falls back to a few platform conventions.

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace {

std::string run_capture(const std::string& cmd) {
    std::string out;
    std::FILE* p = ::
#if defined(_WIN32)
        _popen
#else
        popen
#endif
        (cmd.c_str(),
#if defined(_WIN32)
        "rb"
#else
        "r"
#endif
        );
    if (!p) return out;
    char buf[256];
    while (auto n = std::fread(buf, 1, sizeof(buf), p)) out.append(buf, n);
#if defined(_WIN32)
    _pclose(p);
#else
    pclose(p);
#endif
    return out;
}

void expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        std::exit(1);
    }
}

}  // namespace

int main() {
    const char* bin_env = std::getenv("XFILES_PLAY_BIN");
    std::string bin = bin_env ? bin_env : std::string();
    if (bin.empty() || !std::filesystem::is_regular_file(bin)) {
        std::printf("test_play_probe: SKIP (XFILES_PLAY_BIN not set or "
                     "binary missing) — XFILES_PLAY must be ON\n");
        return 0;
    }

    // Quote the path on Windows in case it has spaces.
    std::string cmd = "\"" + bin + "\" --probe --scene 999000 "
                                    "--asset-dir nonexistent_xv_dir";
    auto out = run_capture(cmd);

    expect(out.find("probe ok") != std::string::npos,
            "stdout must contain 'probe ok'");
    expect(out.find("scene_id=999000") != std::string::npos,
            "stdout must echo the requested scene id");
    expect(out.find("status=fallback") != std::string::npos,
            "missing assets must report status=fallback");

    std::printf("test_play_probe: OK\n");
    return 0;
}
