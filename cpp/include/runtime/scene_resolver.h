// SPDX-License-Identifier: MIT
//
// Map a canonical-flow location name (e.g. "Field Office", "Comity") to the
// numeric scene id used by the sibling `XV/<scene_id>.{xmv,HOT}` files.
//
// The engine harvests inline labels of the shape `Video:Node N:Location:...`
// when it walks the HDB records (see `xfiles_engine.cpp` PART 2/PART 5). The
// `SceneResolver` cross-references these labels with the file names actually
// present on disk in the supplied asset directory. Every mapping that lands
// on a real file is tagged `Certainty::byte_direct`; locations that we have
// no on-disk match for are left absent (the caller falls back to a checker
// pattern or to the boot clip).
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace xfiles::runtime {

enum class Certainty { byte_direct, undetermined };

struct SceneMatch {
    uint32_t   scene_id = 0;
    std::string hot_path;        // absolute path to the .HOT file
    std::string video_path;      // absolute path to the .xmv file (may be empty)
    Certainty  certainty = Certainty::undetermined;
};

class SceneResolver {
public:
    // Build the resolver from a directory holding XV/*.HOT and XV/*.xmv
    // assets, plus the list of (scene_id, location) hints harvested from the
    // HDB inline labels. The hints come from the engine's `load_game` —
    // each scene label of the form "Node N:Location:..." contributes one
    // candidate location per known numeric scene id. If a hint resolves to
    // a present file the match is byte-direct; otherwise it is undetermined.
    static SceneResolver from_hints(
        const std::string& asset_dir,
        const std::vector<std::pair<uint32_t, std::string>>& hints);

    // Build the resolver directly from the JSON artifact emitted by
    // `hdb-extract scene-map` (`examples/outputs/scene_asset_map.json`).
    // Each entry's (location, asset_dir, asset_id) is checked against the
    // file system: if the matching `<asset_dir>/<id>.HOT` exists the match
    // is byte-direct, otherwise it stays undetermined. First-wins per
    // location, with byte-direct matches preferred over undetermined ones.
    static SceneResolver from_scene_asset_map(
        const std::string& scene_asset_map_json_path,
        const std::string& asset_dir);

    // Single-scene lookup by exact location string.
    std::optional<SceneMatch> scene_for(const std::string& location) const;

    // Number of byte-direct matches discovered.
    std::size_t byte_direct_count() const;

    // Iterate all locations the resolver knows about (for diagnostics).
    const std::map<std::string, SceneMatch>& all() const { return by_location_; }

private:
    std::map<std::string, SceneMatch> by_location_;
};

}  // namespace xfiles::runtime
