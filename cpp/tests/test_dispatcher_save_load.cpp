// SPDX-License-Identifier: MIT
//
// Round-trip the dispatcher's VariableState through the save/load JSON path:
//   1. Apply a few effect_summary strings via apply_effect.
//   2. Save the state to a temp JSON file.
//   3. Mutate the live state into something different.
//   4. Load the saved file back and confirm the live state matches the
//      snapshot taken at step 2 (values + counters).

#include "runtime/dispatcher.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

void expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        std::exit(1);
    }
}

}  // namespace

int main() {
    namespace dsp = xfiles::runtime::dispatcher;
    namespace fs = std::filesystem;

    dsp::VariableState vs;
    expect(dsp::apply_effect("set bAIComity_GetLaptop = true", vs),
            "apply true effect must succeed");
    expect(dsp::apply_effect("set bAITest_Counter = 42", vs),
            "apply int effect must succeed");
    expect(dsp::apply_effect("set bAIElsewhere_Done = false", vs),
            "apply false effect must succeed");
    expect(vs.values.size() == 3, "vs must hold 3 entries");
    expect(vs.set_true_count == 1 && vs.set_false_count == 1 &&
            vs.set_int_count == 1, "counters must be 1/1/1");

    fs::path tmp = fs::temp_directory_path() / "xfiles_play_save_test.json";
    expect(dsp::save_state_json(tmp.string(), vs), "save must succeed");
    expect(fs::is_regular_file(tmp), "save file must exist on disk");

    // Mutate the live state into something else.
    vs.values["should_be_overwritten_by_load"] = "true";
    vs.set_true_count = 999;

    dsp::VariableState restored;
    expect(dsp::load_state_json(tmp.string(), restored), "load must succeed");
    expect(restored.values.size() == 3, "restored entry count");
    expect(restored.values["bAIComity_GetLaptop"] == "true", "true value");
    expect(restored.values["bAITest_Counter"] == "42", "int value");
    expect(restored.values["bAIElsewhere_Done"] == "false", "false value");
    expect(restored.set_true_count == 1, "true counter");
    expect(restored.set_false_count == 1, "false counter");
    expect(restored.set_int_count == 1, "int counter");

    // Loading a missing file must fail cleanly (no exception, no exit).
    fs::path missing = fs::temp_directory_path() /
                        "xfiles_play_save_test_DOES_NOT_EXIST.json";
    fs::remove(missing);
    dsp::VariableState empty;
    expect(!dsp::load_state_json(missing.string(), empty),
            "missing file load must return false");

    fs::remove(tmp);
    std::printf("test_dispatcher_save_load: OK\n");
    return 0;
}
