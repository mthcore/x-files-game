// SPDX-License-Identifier: MIT
//
// HSPT decoder — byte-direct parser for the sibling `XV/<scene_id>.HOT`
// files. Header: "HSPT" + count(u32 LE). Entry (20 B): type/z-order (u32 LE) +
// four i16 LE screen coordinates (x_min, y_min, x_max, y_max) + two u32 LE
// action ids. Shared between the headless reference engine and the SDL2
// playable shell.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace xfiles::runtime {

struct Hotspot {
    int      index = 0;
    uint32_t type_zorder = 0;
    int16_t  x_min = 0, y_min = 0, x_max = 0, y_max = 0;
    uint32_t action_id_1 = 0, action_id_2 = 0;
};

// Parse a buffer holding the contents of one .HOT file. Returns an empty
// vector if the buffer fails the strict `8 + n*20` size invariant or the
// magic does not match.
std::vector<Hotspot> load_hot_bytes(const std::vector<uint8_t>& b);

// Read a .HOT file from disk. Returns an empty vector on I/O failure or on
// any invariant violation (no exceptions — the caller decides what to do).
std::vector<Hotspot> load_hot_file(const std::string& path);

// Hit-test a screen point against the rect list. Returns the index of the
// first rect (in z-order — caller is expected to sort by `type_zorder` if
// needed) that contains the point, or -1 if none matched.
int hit_test(const std::vector<Hotspot>& rects, int x, int y);

}  // namespace xfiles::runtime
