// SPDX-License-Identifier: MIT
#include "runtime/scene_resolver.h"
#include "runtime/mini_json.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>

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

SceneResolver SceneResolver::from_scene_asset_map(
    const std::string& json_path,
    const std::string& asset_dir) {
    namespace J = ::xfiles::engine::json;
    std::vector<std::pair<uint32_t, std::string>> hints;
    std::string buf;
    if (!J::load_file(json_path, buf)) return from_hints(asset_dir, hints);
    J::Parser parser(buf.data(), buf.size());
    J::Value root;
    if (!parser.parse(root) || !root.is_obj()) {
        return from_hints(asset_dir, hints);
    }
    const J::Value& entries = root.at("entries");
    if (!entries.is_arr()) return from_hints(asset_dir, hints);
    // Preserve first-seen order — the JSON is emitted sorted by asset_id, so
    // the first interactive scene per location ends up at the smallest id.
    for (const J::Value& e : entries.as_arr()) {
        if (!e.is_obj()) continue;
        const J::Value& loc = e.at("location");
        const J::Value& aid = e.at("asset_id");
        if (!loc.is_str() || !aid.is_num()) continue;
        hints.emplace_back(static_cast<uint32_t>(aid.as_int()), loc.as_str());
    }
    return from_hints(asset_dir, hints);
}

std::size_t SceneResolver::byte_direct_count() const {
    std::size_t n = 0;
    for (const auto& [_, m] : by_location_) {
        if (m.certainty == Certainty::byte_direct) ++n;
    }
    return n;
}

}  // namespace xfiles::runtime
