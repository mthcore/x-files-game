// SPDX-License-Identifier: MIT
//
// Headless dispatcher — interpret each trigger's byte-direct ``effect_summary``
// (the closed grammar ``set <var> = true | false | <int>``) and mutate a
// VariableState map. Shared between the reference engine (which dispatches
// the canonical 29-step flow) and the SDL2 playable shell (which dispatches
// per-scene on enter).
#pragma once

#include <cctype>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "runtime/mini_json.h"

namespace xfiles::runtime::dispatcher {

struct VariableState {
    std::map<std::string, std::string> values;
    int set_true_count = 0;
    int set_false_count = 0;
    int set_int_count = 0;
    int uninterpreted_count = 0;
};

// Apply one effect_summary string to the variable state. Returns true if the
// closed grammar matched and a mutation occurred; false otherwise (the call
// still increments `vs.uninterpreted_count` in that case so callers can keep
// a running tally without re-parsing).
inline bool apply_effect(const std::string& effect, VariableState& vs) {
    auto skip_ws = [](const std::string& s, std::size_t i) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        return i;
    };
    std::size_t i = skip_ws(effect, 0);
    if (effect.compare(i, 4, "set ") != 0) {
        ++vs.uninterpreted_count; return false;
    }
    i += 4;
    i = skip_ws(effect, i);
    std::size_t name_start = i;
    while (i < effect.size() &&
           (std::isalnum(static_cast<unsigned char>(effect[i])) ||
            effect[i] == '_')) {
        ++i;
    }
    if (i == name_start) { ++vs.uninterpreted_count; return false; }
    std::string name = effect.substr(name_start, i - name_start);
    i = skip_ws(effect, i);
    if (i >= effect.size() || effect[i] != '=') {
        ++vs.uninterpreted_count; return false;
    }
    ++i;
    i = skip_ws(effect, i);
    if (i >= effect.size()) { ++vs.uninterpreted_count; return false; }
    std::string rhs = effect.substr(i);
    while (!rhs.empty() && (rhs.back() == ' ' || rhs.back() == '\t' ||
                             rhs.back() == '\r' || rhs.back() == '\n')) {
        rhs.pop_back();
    }
    if (rhs == "true")  { vs.values[name] = "true";  ++vs.set_true_count;  return true; }
    if (rhs == "false") { vs.values[name] = "false"; ++vs.set_false_count; return true; }
    bool is_int = !rhs.empty();
    std::size_t j = (rhs[0] == '-') ? 1 : 0;
    if (j == rhs.size()) is_int = false;
    for (; j < rhs.size(); ++j) {
        if (!std::isdigit(static_cast<unsigned char>(rhs[j]))) {
            is_int = false; break;
        }
    }
    if (is_int) { vs.values[name] = rhs; ++vs.set_int_count; return true; }
    ++vs.uninterpreted_count;
    return false;
}

// A loaded slice of `game_definition.json` keyed for per-scene lookups: for
// each location, the list of effect_summary strings the dispatcher would
// fire when the player enters / interacts with that scene.
struct ScenesByLocation {
    // location -> list of (trigger_id, effect_summary) pairs
    std::map<std::string,
             std::vector<std::pair<std::string, std::string>>> by_location;
};

// One step of the canonical 29-step walkthrough flow.
struct FlowStep {
    int step = 0;
    std::string day;
    std::string location;
};

// Parse `flow.days[].scenes[]` and return the flat 29-step sequence in
// canonical order. Used by the playable shell to walk locations on SPACE.
inline std::vector<FlowStep> load_flow_steps(const std::string& json_path) {
    namespace J = ::xfiles::engine::json;
    std::vector<FlowStep> out;
    std::string buf;
    if (!J::load_file(json_path, buf)) return out;
    J::Parser parser(buf.data(), buf.size());
    J::Value root;
    if (!parser.parse(root) || !root.is_obj()) return out;
    auto str_at = [](const J::Value& v, const std::string& k) {
        const J::Value& f = v.at(k);
        if (f.is_str()) return f.as_str();
        if (f.is_obj() && f.at("value").is_str()) return f.at("value").as_str();
        return std::string();
    };
    auto int_at = [](const J::Value& v, const std::string& k) -> long long {
        const J::Value& f = v.at(k);
        if (f.is_num()) return f.as_int();
        if (f.is_obj() && f.at("value").is_num()) return f.at("value").as_int();
        return 0;
    };
    const J::Value& days = root.at("flow").at("days");
    if (!days.is_arr()) return out;
    for (const J::Value& d : days.as_arr()) {
        const std::string day_name = str_at(d, "day");
        const J::Value& scs = d.at("scenes");
        if (!scs.is_arr()) continue;
        for (const J::Value& s : scs.as_arr()) {
            FlowStep fs;
            fs.step = static_cast<int>(int_at(s, "step"));
            fs.day = day_name;
            fs.location = str_at(s, "location");
            if (!fs.location.empty()) out.push_back(std::move(fs));
        }
    }
    return out;
}

// Parse `examples/outputs/game_definition.json` and emit the per-location
// trigger effect map. The flat-grammar `effect_summary` strings are read
// straight from the JSON; their honesty contract (closed grammar, otherwise
// undetermined) is preserved.
inline ScenesByLocation load_scenes_by_location(const std::string& json_path) {
    namespace J = ::xfiles::engine::json;
    ScenesByLocation out;
    std::string buf;
    if (!J::load_file(json_path, buf)) return out;
    J::Parser parser(buf.data(), buf.size());
    J::Value root;
    if (!parser.parse(root) || !root.is_obj()) return out;

    // 1) trigger_id -> effect_summary
    std::map<std::string, std::string> effect_by_id;
    const J::Value& trigs = root.at("triggers");
    if (trigs.is_arr()) {
        for (const J::Value& t : trigs.as_arr()) {
            if (!t.is_obj()) continue;
            auto str_at = [](const J::Value& v, const std::string& k) {
                const J::Value& f = v.at(k);
                if (f.is_str()) return f.as_str();
                if (f.is_obj() && f.at("value").is_str())
                    return f.at("value").as_str();
                return std::string();
            };
            std::string tid = str_at(t, "trigger_id");
            std::string eff = str_at(t, "effect_summary");
            if (!tid.empty()) effect_by_id[tid] = eff;
        }
    }

    // 2) location -> trigger_refs[] -> (trigger_id, effect_summary)
    const J::Value& scenes = root.at("scenes");
    if (scenes.is_obj()) {
        for (const auto& kv : scenes.as_obj()) {
            const std::string& loc = kv.first;
            const J::Value& sc = kv.second;
            const J::Value& refs = sc.at("trigger_refs");
            if (!refs.is_arr()) continue;
            for (const J::Value& r : refs.as_arr()) {
                std::string tid;
                if (r.is_str()) tid = r.as_str();
                else if (r.is_obj() && r.at("value").is_str())
                    tid = r.at("value").as_str();
                if (tid.empty()) continue;
                auto it = effect_by_id.find(tid);
                std::string eff = (it != effect_by_id.end()) ? it->second
                                                                : std::string();
                out.by_location[loc].emplace_back(tid, eff);
            }
        }
    }
    return out;
}

// Fire every trigger registered for `location`. Returns the list of variable
// mutations produced (var_name, new_value) and updates `vs` in place. Caller
// can call this on scene enter (the play shell does) or per-step (the
// reference engine's flow walker does).
inline std::vector<std::pair<std::string, std::string>>
dispatch_scene(const ScenesByLocation& map,
                const std::string& location,
                VariableState& vs) {
    std::vector<std::pair<std::string, std::string>> muts;
    auto it = map.by_location.find(location);
    if (it == map.by_location.end()) return muts;
    for (const auto& [tid, eff] : it->second) {
        std::size_t before = vs.values.size();
        std::string snapshot_value;
        bool had_key = false;
        // Track if the var existed before (so we can detect overwrite vs new).
        if (apply_effect(eff, vs)) {
            // Best-effort: the variable name appears just after "set ".
            std::size_t sp = eff.find("set ");
            if (sp != std::string::npos) {
                std::size_t s = sp + 4;
                while (s < eff.size() &&
                       (eff[s] == ' ' || eff[s] == '\t')) ++s;
                std::size_t e = s;
                while (e < eff.size() &&
                       (std::isalnum(static_cast<unsigned char>(eff[e])) ||
                        eff[e] == '_')) ++e;
                std::string name = eff.substr(s, e - s);
                auto vit = vs.values.find(name);
                if (vit != vs.values.end()) {
                    muts.emplace_back(name, vit->second);
                }
            }
        }
        (void)before;
        (void)snapshot_value;
        (void)had_key;
    }
    return muts;
}

}  // namespace xfiles::runtime::dispatcher
