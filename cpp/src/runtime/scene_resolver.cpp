// SPDX-License-Identifier: MIT
#include "runtime/scene_resolver.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace xfiles::runtime {

namespace {
std::string join(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    char last = dir.back();
    if (last == '/' || last == '\\') return dir + name;
    return dir + "/" + name;
}

bool file_exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}
}  // namespace

SceneResolver SceneResolver::from_hints(
    const std::string& asset_dir,
    const std::vector<std::pair<uint32_t, std::string>>& hints) {
    SceneResolver r;
    for (const auto& [scene_id, location] : hints) {
        if (location.empty()) continue;
        const std::string hot_path =
            join(asset_dir, std::to_string(scene_id) + ".HOT");
        const std::string xmv_path =
            join(asset_dir, std::to_string(scene_id) + ".xmv");
        SceneMatch m;
        m.scene_id = scene_id;
        m.hot_path = hot_path;
        m.video_path = file_exists(xmv_path) ? xmv_path : std::string();
        m.certainty = file_exists(hot_path) ? Certainty::byte_direct
                                              : Certainty::undetermined;
        // first-wins: keep the first byte-direct match per location
        auto it = r.by_location_.find(location);
        if (it == r.by_location_.end()) {
            r.by_location_[location] = std::move(m);
        } else if (it->second.certainty == Certainty::undetermined &&
                   m.certainty == Certainty::byte_direct) {
            it->second = std::move(m);
        }
    }
    return r;
}

std::optional<SceneMatch> SceneResolver::scene_for(
    const std::string& location) const {
    auto it = by_location_.find(location);
    if (it == by_location_.end()) return std::nullopt;
    if (it->second.certainty != Certainty::byte_direct) return std::nullopt;
    return it->second;
}

std::size_t SceneResolver::byte_direct_count() const {
    std::size_t n = 0;
    for (const auto& [_, m] : by_location_) {
        if (m.certainty == Certainty::byte_direct) ++n;
    }
    return n;
}

}  // namespace xfiles::runtime
