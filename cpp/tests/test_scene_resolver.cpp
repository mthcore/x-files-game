// SPDX-License-Identifier: MIT
//
// scene_resolver unit test. Builds a fake asset directory in the system temp
// folder with synthetic .HOT files (just the minimal HSPT header so the
// resolver registers them as present) and verifies that hints mapping a
// location to those numeric ids are reported `byte_direct`. Locations whose
// hint files are absent are reported `nullopt`.

#include "runtime/hsp_loader.h"
#include "runtime/scene_resolver.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

void write_hot_file(const std::filesystem::path& path, uint32_t entry_count) {
    std::ofstream f(path, std::ios::binary);
    const char magic[4] = { 'H', 'S', 'P', 'T' };
    f.write(magic, 4);
    f.write(reinterpret_cast<const char*>(&entry_count), sizeof(entry_count));
    // 20 bytes per entry, zero-filled is fine for the loader's invariants.
    std::vector<char> body(static_cast<std::size_t>(entry_count) * 20, 0);
    if (!body.empty()) f.write(body.data(), static_cast<std::streamsize>(body.size()));
}

void expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        std::exit(1);
    }
}

}  // namespace

int main() {
    namespace fs = std::filesystem;

    fs::path tmp = fs::temp_directory_path() / "xfiles_scene_resolver_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    // Two synthetic scenes on disk, one missing.
    write_hot_file(tmp / "19672.HOT", 9);
    write_hot_file(tmp / "20001.HOT", 3);
    // 99999 deliberately absent — the resolver must classify it as undetermined.

    std::vector<std::pair<uint32_t, std::string>> hints = {
        {19672, "Field Office"},
        {20001, "Comity"},
        {99999, "Hangar"},
    };
    auto r = xfiles::runtime::SceneResolver::from_hints(tmp.string(), hints);

    auto fo = r.scene_for("Field Office");
    expect(fo.has_value(), "Field Office must resolve");
    expect(fo->scene_id == 19672, "Field Office scene_id mismatch");
    expect(fo->certainty == xfiles::runtime::Certainty::byte_direct,
            "Field Office must be byte-direct");

    auto cm = r.scene_for("Comity");
    expect(cm.has_value(), "Comity must resolve");
    expect(cm->scene_id == 20001, "Comity scene_id mismatch");

    auto hg = r.scene_for("Hangar");
    expect(!hg.has_value(),
            "Hangar must NOT resolve (file absent, must stay undetermined)");

    expect(r.byte_direct_count() == 2,
            "byte-direct count must be exactly 2");

    // Round-trip the .HOT file through the runtime loader to confirm the
    // shared parser works on the same fixture.
    auto rects = xfiles::runtime::load_hot_file((tmp / "19672.HOT").string());
    expect(rects.size() == 9, "HSPT entry count mismatch on round-trip");

    fs::remove_all(tmp);
    std::printf("test_scene_resolver: OK (2/3 byte-direct, 9 rects round-trip)\n");
    return 0;
}
