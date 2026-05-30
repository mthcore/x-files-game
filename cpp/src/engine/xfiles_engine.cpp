// SPDX-License-Identifier: MIT
//
// xfiles_engine — a small, self-contained REFERENCE GAME ENGINE for
// *The X-Files: The Game* (1997, HyperBole Studios), built on top of
// the byte-direct HDB decoder in this package.
//
// It is a "pseudo-engine": it shows, end to end, HOW THE GAME IS STORED and HOW
// IT RUNS, without depending on any of the original game's graphics/video code.
// You give it `XFILES.HDB` (+ the sibling `XV/*.HOT` files), and it:
//
//   1. opens the HDB container and walks every record;
//   2. rebuilds the game's object model (scenes, hotspots, triggers,
//      conditions, actions, conversations, variables);
//   3. prints the reconstructed game flow and runs a headless "game loop" that
//      demonstrates the runtime logic (present scene → hit-test hotspots →
//      fire actions → evaluate triggers → navigate).
//
// Each location is modelled as a small STATE MACHINE — a phase variable plus
// checkpoint flags (PART 7) — which is how the game gates progression and
// fixes the scene order (see docs/GAME_FLOW.md).
//
// Everything here is derived from the on-disk format of the data files. The few
// places where a runtime-internal value (an inter-object handle) cannot be
// resolved from the files alone are clearly marked `UNRESOLVED` — never guessed.
//
// Build (see cpp/CMakeLists.txt):  links against the `hdb_decoder` static lib.
// Run:  xfiles_engine /path/to/XFILES.HDB
//
// ===========================================================================

#include "hdb/hdb_container.h"
#include "nl/hdb_record_index.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace xfiles {
namespace engine {

// the decoder's record types live in xfiles::nl
using nl::RecordRef;
using nl::HDBRecordIndex;

// ===========================================================================
// PART 1 — THE HDB CONTAINER (how to read the file)
// ===========================================================================
//
// XFILES.HDB is a single ~6 MB file holding the ENTIRE game: scenes, hotspots,
// dialogue trees, triggers, actions, NPC conversations, emails, decision graphs.
// It is serialized with NeoLogic's NeoPersist 3.0 library (the persistence layer
// of HyperBole's VirtualCinema engine).
//
// Layout:
//   * 32-byte file header, then a grid of 256-byte PAGES.
//   * Each page's first byte is a tag. Data pages are 0xC0..0xCF; structural
//     B-tree pages are 0xC2/0xC3/0xD2; 0x00 pages are zero padding.
//   * Inside a data page, records are laid out on an 8-byte grid. Each record
//     starts with an 8-byte CNeoPersist MARKER:
//
//         +0  flags1   (0xC0..0xCF)
//         +1  sub_tag
//         +2..+5  version  =  00 00 00 01   (kNeoVersionDefault — the invariant
//                                            we scan for)
//         +6..+7  class_id (big-endian u16)  <-- identifies the object's class
//
//     The record's PAYLOAD runs from marker+8 to the next marker (or page end).
//
// The class_id is the key to everything: it tells you what kind of object the
// payload encodes. The registered VC* classes are 0x27..0x5C.

// ---------------------------------------------------------------------------
// Class IDs (the VirtualCinema object model). These are the values found at
// marker bytes +6..+7. Names are the engine's own class names.
// ---------------------------------------------------------------------------
enum ClassId : uint16_t {
    kVCTitle           = 0x27, // a named title / state token
    kVCNode            = 0x28, // a story NODE (an ordered moment of the game)
    kVCLocation        = 0x29, // a physical place (e.g. "Field Office")
    kVCViewPoint       = 0x2A,
    kVCView            = 0x2B,
    kVCCharacter       = 0x2C, // an NPC
    kVCCharView        = 0x2D,
    kVCName            = 0x2F,
    kVCConversation    = 0x31, // a dialogue exchange
    kVCConversationList= 0x32,
    kVCNav             = 0x33, // a navigation link (move between places)
    kVCNavList         = 0x34,
    kVCAssetRef        = 0x35, // reference to a media asset (video/audio/pict)
    kVCExplorable      = 0x37,
    kVCHotSpot         = 0x39, // a clickable region (geometry in the .HOT files)
    kVCHotSpotList     = 0x3A,
    kVCStdHotSpotList  = 0x3B,
    kVCAssetCategory   = 0x3C,
    kVCCursor          = 0x3D,
    kVCAction          = 0x41, // an action (changes game state / plays media)
    kVCActionList      = 0x42,
    kVCBlob            = 0x43, // an opaque value blob (a stored variable value)
    kVCInterfaceItem   = 0x4F,
    kVCTrigger         = 0x51, // a TRIGGER: conditions -> actions -> targets
    kVCTriggerList     = 0x52,
    kVCVariable        = 0x53, // a named game-state variable
    kVCStdAction       = 0x54, // a "standard" action: 3 dword params
    kVCGameState       = 0x57, // the global game-state flag block
    kVCEmail           = 0x5A,
    kVCEmailRead       = 0x5B,
    kVCEmailPending    = 0x5C,
};

// CNeoPart sub-records (NOT registered classes): the small 16-byte structures a
// trigger uses to express conditions/relations. Managed by CNeoPartMgr.
static bool is_part_subrecord(uint16_t cid) {
    return cid == 0x0F || cid == 0x10 || cid == 0x11 || cid == 0x14 ||
           cid == 0x15 || cid == 0x1B || cid == 0x1C || cid == 0x1D;
}

static const char* class_name(uint16_t cid) {
    switch (cid) {
        case kVCTitle: return "VCTitle";       case kVCNode: return "VCNode";
        case kVCLocation: return "VCLocation";  case kVCCharacter: return "VCCharacter";
        case kVCConversation: return "VCConversation"; case kVCNav: return "VCNav";
        case kVCAssetRef: return "VCAssetRef";  case kVCHotSpot: return "VCHotSpot";
        case kVCHotSpotList: return "VCHotSpotList"; case kVCAction: return "VCAction";
        case kVCBlob: return "VCBlob";          case kVCTrigger: return "VCTrigger";
        case kVCVariable: return "VCVariable";  case kVCStdAction: return "VCStdAction";
        case kVCGameState: return "VCGameState"; case kVCEmail: return "VCEmail";
        default: break;
    }
    if (is_part_subrecord(cid)) return "CNeoPart";
    return "(other)";
}

// ===========================================================================
// PART 2 — READING FIELDS FROM A RECORD
// ===========================================================================
//
// Big-endian helpers — NeoPersist stores multi-byte integers big-endian on disk.
static uint16_t be16(const uint8_t* p) { return (uint16_t(p[0]) << 8) | p[1]; }
static uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8) | p[3];
}

// The single longest printable run inside a record payload is its inline LABEL —
// a self-describing string the authoring tool baked into the record, e.g.
//   "Video:Node 1:Field Office:Shanks \"Come in\" 1A/FO/IN2 fo_in2.mov"
// This is read directly from the bytes; it carries the scene/character/dialogue.
static std::string primary_label(const uint8_t* payload, std::size_t size) {
    std::string best, cur;
    for (std::size_t i = 0; i < size; ++i) {
        uint8_t c = payload[i];
        if (c >= 0x20 && c <= 0x7e) { cur.push_back(char(c)); }
        else { if (cur.size() > best.size()) best = cur; cur.clear(); }
    }
    if (cur.size() > best.size()) best = cur;
    return best.size() >= 4 ? best : std::string();
}

// ===========================================================================
// PART 3 — THE CNeoPart (a trigger condition / relation), 16-byte payload
// ===========================================================================
//
// A trigger's logic is expressed by CNeoPart sub-records. The verified 16-byte
// layout:
//     +0   ref_a    (BE u16)  operand-A handle
//     +5   class_a  (u8)       operand-A class id  (e.g. 0x43 VCBlob)
//     +8   ref_b    (BE u16)   operand-B handle    (0 when single-target)
//     +14  relation (u8)       discriminator (2 values: 0x01 / 0x11)
//     +15  class_b  (u8)       operand-B class id  (0 when single-target)
//
// A part with two typed operands is a BINARY RELATION (relates A and B); a part
// with only operand A is a MONO-TARGET reference. The `ref_a/ref_b` handles are
// internal object handles whose target offset is NOT recoverable from the file
// alone — we expose them raw and mark them UNRESOLVED.
struct Part {
    uint16_t ref_a = 0, ref_b = 0;
    uint8_t  class_a = 0, class_b = 0, relation = 0;
    bool     binary = false;
    static Part read(const uint8_t* p, std::size_t size) {
        Part t;
        if (size < 16) return t;
        t.ref_a = be16(p + 0); t.class_a = p[5];
        t.ref_b = be16(p + 8); t.relation = p[14]; t.class_b = p[15];
        t.binary = (t.class_b != 0 || t.ref_b != 0);
        return t;
    }
};

// ===========================================================================
// PART 4 — THE GAME OBJECT MODEL (rebuilt from the records)
// ===========================================================================
struct Hotspot {          // from the scene's .HOT file (screen geometry)
    int      index;
    uint32_t type_zorder; // raw z-order/layer number
    int16_t  x_min, y_min, x_max, y_max;   // 640x480 screen rect
    uint32_t action_id_1, action_id_2;     // what fires on click
};

struct Trigger {
    std::size_t          offset;     // record offset in the HDB
    std::vector<Part>    relations;  // binary-relation parts (conditions-like)
    std::vector<Part>    targets;    // mono-target parts (objects it touches)
    std::vector<std::string> affects;// labels of real VC* objects it wires to
};

struct GameModel {
    // every record, grouped by class
    std::map<uint16_t, std::vector<RecordRef>> by_class;
    // scene-moments keyed "Node N / Location" with their ordered labels
    std::map<std::string, std::vector<std::string>> scenes;
    std::vector<Trigger> triggers;
    // hotspots keyed by scene/node id (the .HOT file stem)
    std::map<std::string, std::vector<Hotspot>> hotspots;
    int variable_count = 0, action_count = 0;
};

// ===========================================================================
// PART 5 — LOADING THE GAME FROM THE HDB
// ===========================================================================
//
// Walk every record once: index by class, harvest scene labels, decode triggers.
static GameModel load_game(const uint8_t* hdb, std::size_t hdb_size) {
    GameModel g;
    HDBRecordIndex index(hdb, hdb_size);

    for (const RecordRef& r : index.all()) {
        g.by_class[r.class_id].push_back(r);
        if (r.payload_size < 12) continue;
        const uint8_t* payload = hdb + r.byte_offset + 8;

        // --- scene labels: a label that names a "Node N" + a location is a
        //     distinct scene-moment. The same place at different Nodes is a
        //     different scene (Node 1 / Field Office != Node 2 / Field Office).
        std::string label = primary_label(payload, r.payload_size);
        std::size_t np = label.find("Node ");
        if (np != std::string::npos) {
            // scene key = "Node <number>" (digits right after "Node ")
            std::string nn = "Node ";
            for (char c : label.substr(np + 5)) {
                if (c >= '0' && c <= '9') nn.push_back(c); else break;
            }
            if (nn.size() > 5) g.scenes[nn].push_back(label);
        }

        if (r.class_id == kVCVariable) g.variable_count++;
        if (r.class_id == kVCAction || r.class_id == kVCStdAction) g.action_count++;
    }

    // --- triggers: each VCTrigger record holds pairs that point at CNeoPart
    //     sub-records (its conditions/targets) and at real VC* objects (affects).
    for (const RecordRef& tr : g.by_class[kVCTrigger]) {
        Trigger t; t.offset = tr.byte_offset;
        if (tr.payload_size < 24) { g.triggers.push_back(t); continue; }
        const uint8_t* payload = hdb + tr.byte_offset + 8;
        // pairs region begins at payload +0x18: [byte_offset 4B BE][neoid 4B BE]
        for (std::size_t m = 0x18; m + 8 <= tr.payload_size; m += 8) {
            uint32_t boff = be32(payload + m);
            if (boff < 100 || boff + 8 > hdb_size) continue;
            // does a real marker sit at/near boff?
            if (hdb[boff + 2] != 0 || hdb[boff + 3] != 0 ||
                hdb[boff + 4] != 0 || hdb[boff + 5] != 1) continue;
            uint16_t cid = be16(hdb + boff + 6);
            if (is_part_subrecord(cid)) {
                Part p = Part::read(hdb + boff + 8, 16);
                if (p.binary) t.relations.push_back(p); else t.targets.push_back(p);
            } else if (cid >= 0x27 && cid <= 0x5C) {
                std::string lbl = primary_label(hdb + boff + 8, 64);
                t.affects.push_back(std::string(class_name(cid)) +
                                    (lbl.empty() ? "" : (": " + lbl)));
            }
        }
        g.triggers.push_back(t);
    }
    return g;
}

// ---------------------------------------------------------------------------
// Hotspot geometry lives in the sibling XV/<scene_id>.HOT files (the 'HSPT'
// format), NOT in the HDB. Header: 'HSPT' + count(u32 LE). Entry (20B):
//   type/z-order(u32 LE) + x_min,y_min,x_max,y_max (i16 LE x4) + action1,action2.
// ---------------------------------------------------------------------------
static std::vector<Hotspot> load_hot_file(const std::vector<uint8_t>& b) {
    std::vector<Hotspot> out;
    auto le32 = [](const uint8_t* p) {
        return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
               (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    };
    auto le16 = [](const uint8_t* p) { return int16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8)); };
    if (b.size() < 8 || std::memcmp(b.data(), "HSPT", 4) != 0) return out;
    uint32_t n = le32(&b[4]);
    if (b.size() != 8u + n * 20u) return out;          // strict size invariant
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t* e = &b[8 + i * 20];
        Hotspot h; h.index = int(i);
        h.type_zorder = le32(e);
        h.x_min = le16(e + 4); h.y_min = le16(e + 6);
        h.x_max = le16(e + 8); h.y_max = le16(e + 10);
        h.action_id_1 = le32(e + 12); h.action_id_2 = le32(e + 16);
        out.push_back(h);
    }
    return out;
}

// ===========================================================================
// PART 6 — THE GAME LOOP (how the reconstructed model RUNS)
// ===========================================================================
//
// The runtime model of VirtualCinema, expressed against the data above:
//
//   for each story Node (1..N), in order:
//     present the Node's scene-moment:
//       - play its intro video (the scene's media asset)
//       - draw the still backdrop
//     loop:
//       - read mouse; hit-test the cursor against the scene's HOTSPOT rects
//         (from the .HOT file). The first rect (by z-order) that contains the
//         cursor is the active hotspot.
//       - on click: dispatch the hotspot's action_id (play a video, open a
//         conversation, pick up an item, navigate via a VCNav, ...).
//       - after any state change, evaluate the scene's TRIGGERS:
//           for each trigger:
//             if ALL its condition parts hold (operand_a <relation> operand_b)
//               then apply its actions to the objects it `affects`.
//         (The relation operator and the operand handles are runtime-internal;
//          this reference engine evaluates the STRUCTURE and reports them.)
//       - a VCNav action changes the current Node/Location -> next scene-moment.
//
// Below we run this loop headlessly over the loaded model and print what would
// happen, which is exactly the reconstruction a re-implementation needs.
static const Hotspot* hit_test(const std::vector<Hotspot>& hs, int x, int y) {
    const Hotspot* best = nullptr;
    for (const Hotspot& h : hs) {
        if (x >= h.x_min && x <= h.x_max && y >= h.y_min && y <= h.y_max) {
            if (!best || h.type_zorder > best->type_zorder) best = &h; // top layer
        }
    }
    return best;
}

// ===========================================================================
// PART 7 — THE SCENE STATE MACHINE (what drives progression)
// ===========================================================================
//
// Each explorable LOCATION is a small state machine, byte-direct from the
// savegame (XFILES.GAM) variables:
//
//   * a PHASE variable  cAI<Location>WhereAreWe  — the scene's current step;
//   * CHECKPOINT flags   bAI<Location>_<Action>  — objectives done in the scene.
//
// The loop in PART 6 drives it: clicking a hotspot completes a checkpoint
// (sets a bAI flag); when the scene's required checkpoints hold, the phase
// advances; reaching the final phase unlocks the next location (travelled to
// via the in-game PDA map). Example, the Field Office:
//   cAIFieldOfficeWhereAreWe:  Phone -> APB/Case Files -> Equipment ->
//                              Warehouse -> Node 3 -> Done
// The canonical 6-day, 29-step scene order + per-scene checkpoints are in
// examples/outputs/game_flow.json and all_scenes.json (see docs/GAME_FLOW.md).
struct SceneState {
    std::string location;        // e.g. "Field Office"
    char        phase = 0;       // current cAI<Location>WhereAreWe code
    // checkpoint flag name -> done?  (bAI<Location>_<Action>)
    std::map<std::string, bool> checkpoints;
    bool done() const {          // scene is complete when all checkpoints hold
        for (const auto& kv : checkpoints) if (!kv.second) return false;
        return true;
    }
};

// A dialogue exchange (VCConversation, class 0x31). The scalar fields below are
// the on-disk grammar (tags e..k); prompt/response are string ids that resolve
// to the localization-DLL dialogue text (see docs/DIALOGUES.md). The exact
// branch wiring is assembled through the NeoPersist list/index (VCConversationList
// 0x32 + the ID-index), so a re-implementation resolves next_id through that
// index, the same way it resolves other inter-object handles.
struct Conversation {
    uint32_t conversation_id = 0;   // 'e'
    uint32_t speaker_topic_id = 0;  // 'f'
    uint32_t prompt_string_id = 0;  // 'g'  -> localization DLL
    uint32_t response_id_1 = 0;     // 'h'  -> localization DLL
    uint32_t response_id_2 = 0;     // 'i'  -> localization DLL
    uint32_t next_conversation_id = 0; // 'j' (branch target, via the index)
    uint8_t  flags = 0;             // 'k'  visited / unlocked
};

// ===========================================================================
// PART 8 — MINIMAL JSON READER (in-tree, no external dep)
// ===========================================================================
//
// A tiny recursive-descent parser tailored to the shape of `game_definition.json`
// and `game_flow.json`. It accepts: null, bool, integer or floating numbers,
// double-quoted strings (with the escapes used by these files: \" \\ \/ \n \r
// \t \b \f), arrays and objects. No exceptions: a parse error leaves the parser
// in a `failed_` state with a one-line diagnostic. The DOM is a Value variant.
namespace json {

// Object/Array are declared as wrapper structs (NOT std::map<std::string,Value>
// directly) so they can be forward-referenced from Value before Value itself is
// complete — std::map requires a complete value type, std::vector does not in
// C++17 but MSVC is stricter. Wrapping the storage behind a struct sidesteps it.
struct Value;
struct Array  { std::vector<Value> v; };
struct Object {
    // ordered insertion for stable iteration order
    std::vector<std::string> keys;
    std::map<std::string, Value> m;
};

struct Value {
    enum Kind { kNull, kBool, kInt, kReal, kStr, kArr, kObj };
    Kind        kind = kNull;
    bool        b   = false;
    long long   i   = 0;
    double      r   = 0.0;
    std::string s;
    std::shared_ptr<Array>  a;
    std::shared_ptr<Object> o;

    bool is_null() const { return kind == kNull; }
    bool is_obj()  const { return kind == kObj && o; }
    bool is_arr()  const { return kind == kArr && a; }
    bool is_str()  const { return kind == kStr; }
    bool is_num()  const { return kind == kInt || kind == kReal; }

    long long as_int() const { return kind == kReal ? (long long)r : i; }
    const std::string& as_str() const { return s; }
    const std::vector<Value>&  as_arr() const {
        static std::vector<Value> e; return a ? a->v : e;
    }
    const std::map<std::string, Value>& as_obj() const {
        static std::map<std::string, Value> e; return o ? o->m : e;
    }

    // safe field accessors: return null Value if absent / wrong shape.
    const Value& at(const std::string& k) const {
        static Value null;
        if (!is_obj()) return null;
        auto it = o->m.find(k);
        return it == o->m.end() ? null : it->second;
    }
    const Value& at(std::size_t idx) const {
        static Value null;
        if (!is_arr() || idx >= a->v.size()) return null;
        return a->v[idx];
    }
    bool has(const std::string& k) const {
        return is_obj() && o->m.find(k) != o->m.end();
    }
};

class Parser {
public:
    Parser(const char* data, std::size_t n) : p_(data), end_(data + n) {}
    bool parse(Value& out) {
        skip_ws();
        if (!parse_value(out)) return false;
        skip_ws();
        if (p_ != end_) return fail("trailing data");
        return true;
    }
    const std::string& error() const { return error_; }

private:
    const char* p_;
    const char* end_;
    std::string error_;

    bool fail(const char* msg) {
        if (error_.empty()) {
            std::ostringstream ss;
            ss << "json parse error: " << msg;
            error_ = ss.str();
        }
        return false;
    }
    void skip_ws() {
        while (p_ < end_) {
            char c = *p_;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++p_;
            else break;
        }
    }
    bool parse_value(Value& v) {
        skip_ws();
        if (p_ >= end_) return fail("eof in value");
        char c = *p_;
        if (c == '{') return parse_object(v);
        if (c == '[') return parse_array(v);
        if (c == '"') return parse_string_val(v);
        if (c == 't' || c == 'f') return parse_bool(v);
        if (c == 'n') return parse_null(v);
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number(v);
        return fail("unexpected character");
    }
    bool parse_null(Value& v) {
        if (end_ - p_ < 4 || std::memcmp(p_, "null", 4) != 0) return fail("expected null");
        p_ += 4; v.kind = Value::kNull; return true;
    }
    bool parse_bool(Value& v) {
        if (end_ - p_ >= 4 && std::memcmp(p_, "true", 4) == 0) {
            p_ += 4; v.kind = Value::kBool; v.b = true; return true;
        }
        if (end_ - p_ >= 5 && std::memcmp(p_, "false", 5) == 0) {
            p_ += 5; v.kind = Value::kBool; v.b = false; return true;
        }
        return fail("expected bool");
    }
    bool parse_number(Value& v) {
        const char* start = p_;
        if (*p_ == '-') ++p_;
        while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        bool is_real = false;
        if (p_ < end_ && *p_ == '.') {
            is_real = true; ++p_;
            while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        }
        if (p_ < end_ && (*p_ == 'e' || *p_ == 'E')) {
            is_real = true; ++p_;
            if (p_ < end_ && (*p_ == '+' || *p_ == '-')) ++p_;
            while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        }
        std::string tok(start, p_ - start);
        if (tok.empty() || tok == "-") return fail("malformed number");
        if (is_real) { v.kind = Value::kReal; v.r = std::strtod(tok.c_str(), nullptr); }
        else         { v.kind = Value::kInt;  v.i = std::strtoll(tok.c_str(), nullptr, 10); }
        return true;
    }
    bool parse_string_raw(std::string& out) {
        if (p_ >= end_ || *p_ != '"') return fail("expected '\"'");
        ++p_;
        while (p_ < end_) {
            char c = *p_++;
            if (c == '"') return true;
            if (c == '\\') {
                if (p_ >= end_) return fail("eof in escape");
                char e = *p_++;
                switch (e) {
                    case '"':  out.push_back('"');  break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/');  break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'u': {
                        if (end_ - p_ < 4) return fail("eof in \\u escape");
                        // accept and pass through as UTF-8 for the BMP subset
                        unsigned code = 0;
                        for (int k = 0; k < 4; ++k) {
                            char h = *p_++;
                            unsigned digit;
                            if      (h >= '0' && h <= '9') digit = h - '0';
                            else if (h >= 'a' && h <= 'f') digit = 10 + (h - 'a');
                            else if (h >= 'A' && h <= 'F') digit = 10 + (h - 'A');
                            else return fail("bad \\u digit");
                            code = (code << 4) | digit;
                        }
                        if (code < 0x80) {
                            out.push_back((char)code);
                        } else if (code < 0x800) {
                            out.push_back((char)(0xC0 | (code >> 6)));
                            out.push_back((char)(0x80 | (code & 0x3F)));
                        } else {
                            out.push_back((char)(0xE0 | (code >> 12)));
                            out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
                            out.push_back((char)(0x80 | (code & 0x3F)));
                        }
                        break;
                    }
                    default: return fail("bad escape");
                }
            } else {
                out.push_back(c);
            }
        }
        return fail("eof in string");
    }
    bool parse_string_val(Value& v) {
        v.kind = Value::kStr;
        return parse_string_raw(v.s);
    }
    bool parse_array(Value& v) {
        if (*p_ != '[') return fail("expected '['");
        ++p_;
        v.kind = Value::kArr;
        v.a = std::make_shared<Array>();
        skip_ws();
        if (p_ < end_ && *p_ == ']') { ++p_; return true; }
        while (true) {
            Value e;
            if (!parse_value(e)) return false;
            v.a->v.push_back(std::move(e));
            skip_ws();
            if (p_ >= end_) return fail("eof in array");
            if (*p_ == ',') { ++p_; skip_ws(); continue; }
            if (*p_ == ']') { ++p_; return true; }
            return fail("expected ',' or ']'");
        }
    }
    bool parse_object(Value& v) {
        if (*p_ != '{') return fail("expected '{'");
        ++p_;
        v.kind = Value::kObj;
        v.o = std::make_shared<Object>();
        skip_ws();
        if (p_ < end_ && *p_ == '}') { ++p_; return true; }
        while (true) {
            skip_ws();
            std::string key;
            if (!parse_string_raw(key)) return false;
            skip_ws();
            if (p_ >= end_ || *p_ != ':') return fail("expected ':'");
            ++p_;
            Value child;
            if (!parse_value(child)) return false;
            if (v.o->m.find(key) == v.o->m.end()) v.o->keys.push_back(key);
            v.o->m[key] = std::move(child);
            skip_ws();
            if (p_ >= end_) return fail("eof in object");
            if (*p_ == ',') { ++p_; continue; }
            if (*p_ == '}') { ++p_; return true; }
            return fail("expected ',' or '}'");
        }
    }
};

inline bool load_file(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss; ss << f.rdbuf();
    out = ss.str();
    return true;
}

} // namespace json

// ===========================================================================
// PART 9 — GAME DEFINITION (mirrors `game_definition.json`)
// ===========================================================================
//
// We load only the slices the validator needs (top-level `flow`, `scenes`,
// `triggers`, `mechanics`). The full DOM stays accessible through the Value*
// pointers held below, but we do not deep-copy 70k lines.
struct ScenePart {
    std::string location;                      // "Field Office"
    std::vector<std::string> trigger_ids;      // from scenes[loc].trigger_refs[]
    int n_triggers_byte_direct = 0;
    std::string phase_variable;                // "cAIFieldOfficeWhereAreWe" (may be empty)
    std::vector<std::string> phase_machine;    // ordered list of phase values
    std::vector<std::string> checkpoints;      // gate names that advance the phase
};

struct TriggerSummary {
    std::string trigger_id;
    std::string scene_scope;
    std::string action_label;
    std::string effect_summary;                 // "set bAIComity_GetLaptop = true"
    std::vector<std::string> vars_written;
};

struct ActionSummary {
    std::string opcode;
    int         count = 0;
};

struct FlowStep {
    int         step = 0;
    std::string day;
    std::string location;
    int         action_count = 0;
};

struct GameDefinition {
    json::Value root;

    // flat 29-step canonical flow (from flow.days[].scenes[])
    std::vector<FlowStep> steps;

    // scenes keyed by location ("Field Office", etc.)
    std::map<std::string, ScenePart> scenes;

    // triggers indexed by trigger_id string
    std::vector<TriggerSummary> triggers;
    std::unordered_map<std::string, std::size_t> trigger_by_id;

    // action vocabulary (opcode -> count)
    std::vector<ActionSummary> actions;

    // stats echoed back for the report
    int scene_count = 0;
    int trigger_count = 0;
    int variable_count = 0;
    int action_count = 0;
    int conversation_count = 0;
    int dialogue_count = 0;
};

static bool load_game_definition(const std::string& path,
                                  GameDefinition& gd,
                                  std::string& err) {
    std::string buf;
    if (!json::load_file(path, buf)) {
        err = "cannot read " + path;
        return false;
    }
    json::Parser parser(buf.data(), buf.size());
    if (!parser.parse(gd.root)) {
        err = parser.error();
        return false;
    }
    if (!gd.root.is_obj()) { err = "root is not an object"; return false; }

    auto unwrap_str = [](const json::Value& v) -> std::string {
        if (v.is_str()) return v.as_str();
        if (v.is_obj() && v.at("value").is_str()) return v.at("value").as_str();
        return {};
    };
    auto unwrap_int = [](const json::Value& v) -> long long {
        if (v.is_num()) return v.as_int();
        if (v.is_obj() && v.at("value").is_num()) return v.at("value").as_int();
        return 0;
    };

    // flow: iterate days[].scenes[] to build the flat 29-step list
    const json::Value& flow = gd.root.at("flow");
    if (flow.at("days").is_arr()) {
        for (const json::Value& day : flow.at("days").as_arr()) {
            const std::string day_name = unwrap_str(day.at("day"));
            if (!day.at("scenes").is_arr()) continue;
            for (const json::Value& sc : day.at("scenes").as_arr()) {
                FlowStep fs;
                fs.step = (int)unwrap_int(sc.at("step"));
                fs.day = day_name;
                fs.location = unwrap_str(sc.at("location"));
                const json::Value& acts = sc.at("actions");
                if (acts.is_obj()) {
                    const json::Value& av = acts.at("value");
                    if (av.is_arr()) fs.action_count = (int)av.as_arr().size();
                    else if (av.is_str() && !av.as_str().empty()) fs.action_count = 1;
                } else if (acts.is_arr()) {
                    fs.action_count = (int)acts.as_arr().size();
                } else if (acts.is_str() && !acts.as_str().empty()) {
                    fs.action_count = 1;
                }
                gd.steps.push_back(std::move(fs));
            }
        }
    }

    // scenes: keyed by location
    for (const auto& kv : gd.root.at("scenes").as_obj()) {
        ScenePart sp;
        sp.location = unwrap_str(kv.second.at("location"));
        if (sp.location.empty()) sp.location = kv.first;
        sp.n_triggers_byte_direct =
            (int)unwrap_int(kv.second.at("n_triggers_byte_direct"));
        if (kv.second.at("trigger_refs").is_arr()) {
            for (const json::Value& tr : kv.second.at("trigger_refs").as_arr()) {
                const std::string id = unwrap_str(tr);
                if (!id.empty()) sp.trigger_ids.push_back(id);
            }
        }
        const json::Value& smv = kv.second.at("state_machine");
        if (smv.is_obj()) {
            sp.phase_variable = unwrap_str(smv.at("phase_variable"));
            if (smv.at("phase_machine").is_arr()) {
                for (const json::Value& p : smv.at("phase_machine").as_arr()) {
                    const std::string s = unwrap_str(p);
                    if (!s.empty()) sp.phase_machine.push_back(s);
                }
            }
            if (smv.at("checkpoints").is_arr()) {
                for (const json::Value& c : smv.at("checkpoints").as_arr()) {
                    const std::string s = unwrap_str(c);
                    if (!s.empty()) sp.checkpoints.push_back(s);
                }
            }
        }
        gd.scenes[kv.first] = std::move(sp);
    }

    // triggers: list keyed by trigger_id
    for (const json::Value& t : gd.root.at("triggers").as_arr()) {
        TriggerSummary ts;
        ts.trigger_id = unwrap_str(t.at("trigger_id"));
        ts.scene_scope = unwrap_str(t.at("scene_scope"));
        ts.action_label = unwrap_str(t.at("action_label"));
        ts.effect_summary = unwrap_str(t.at("effect_summary"));
        if (t.at("vars_written").is_arr()) {
            for (const json::Value& v : t.at("vars_written").as_arr()) {
                const std::string s = unwrap_str(v);
                if (!s.empty()) ts.vars_written.push_back(s);
            }
        }
        if (!ts.trigger_id.empty()) {
            gd.trigger_by_id[ts.trigger_id] = gd.triggers.size();
            gd.triggers.push_back(std::move(ts));
        }
    }

    // action vocabulary (actions_vocabulary.opcodes is an object)
    const json::Value& av = gd.root.at("actions_vocabulary");
    if (av.at("opcodes").is_obj()) {
        for (const auto& kv : av.at("opcodes").as_obj()) {
            ActionSummary as;
            as.opcode = kv.first;
            as.count = (int)unwrap_int(kv.second.at("count"));
            gd.actions.push_back(std::move(as));
        }
    }

    // stats slice
    const json::Value& st = gd.root.at("_stats");
    if (st.is_obj()) {
        gd.scene_count        = (int)unwrap_int(st.at("scene_count"));
        gd.trigger_count      = (int)unwrap_int(st.at("trigger_count"));
        gd.variable_count     = (int)unwrap_int(st.at("variable_count"));
        gd.action_count       = (int)unwrap_int(st.at("opcode_count"));
        gd.conversation_count = (int)unwrap_int(st.at("conversation_count"));
        gd.dialogue_count     = (int)unwrap_int(st.at("dialogue_count"));
    }
    return true;
}

// ===========================================================================
// PART 10 — SCRIPTED PLAYTHROUGH VALIDATOR
// ===========================================================================
//
// For each step in flow.days[].scenes[] (the canonical 29-step ordering),
// verify the step's location exists in scenes{}, the scene's trigger_refs[]
// all resolve in the global trigger list, and the step lists at least one
// player action.
enum class StepStatus { Pass, WalkthroughOnly, Fail };

struct StepReport {
    int          step = 0;
    std::string  day;
    std::string  location;
    int          triggers_referenced = 0;
    int          triggers_resolved = 0;
    int          action_count = 0;
    bool         scene_found = false;
    StepStatus   status = StepStatus::Fail;
    std::string  diagnostic;
};

struct ValidatorTally {
    int pass = 0;
    int walkthrough_only = 0;
    int fail = 0;
};

// ===========================================================================
// PART 11 — HEADLESS DISPATCHER (byte-direct state-variable mutation)
// ===========================================================================
//
// The dispatcher reads each scene's referenced triggers, parses each trigger's
// effect_summary (the byte-direct one-line description emitted by the
// `triggers_full` extractor), and applies the implied variable mutation to a
// VariableState map. The grammar handled here is the closed shape that the
// extractor produces:
//
//     set <var> = true | false | <int>
//
// Any effect_summary that does not match this grammar is reported as
// "uninterpreted" — the dispatcher never invents semantics for it.
struct VariableState {
    std::map<std::string, std::string> values;   // var_name -> "true"/"false"/"<int>"
    int set_true_count = 0;
    int set_false_count = 0;
    int set_int_count = 0;
    int uninterpreted_count = 0;
};

static void apply_effect(const std::string& effect, VariableState& vs) {
    // Trim
    auto skip_ws = [](const std::string& s, std::size_t i) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        return i;
    };
    std::size_t i = skip_ws(effect, 0);
    auto starts = [&](const char* p) {
        std::size_t n = std::strlen(p);
        return effect.compare(i, n, p) == 0;
    };
    if (!starts("set ")) { ++vs.uninterpreted_count; return; }
    i += 4;
    i = skip_ws(effect, i);
    // read var name
    std::size_t name_start = i;
    while (i < effect.size() &&
            (std::isalnum((unsigned char)effect[i]) || effect[i] == '_'))
        ++i;
    if (i == name_start) { ++vs.uninterpreted_count; return; }
    std::string name = effect.substr(name_start, i - name_start);
    i = skip_ws(effect, i);
    if (i >= effect.size() || effect[i] != '=') { ++vs.uninterpreted_count; return; }
    ++i;
    i = skip_ws(effect, i);
    if (i >= effect.size()) { ++vs.uninterpreted_count; return; }
    std::string rhs = effect.substr(i);
    // strip trailing whitespace
    while (!rhs.empty() && (rhs.back() == ' ' || rhs.back() == '\t' ||
                              rhs.back() == '\r' || rhs.back() == '\n'))
        rhs.pop_back();
    if (rhs == "true") { vs.values[name] = "true"; ++vs.set_true_count; return; }
    if (rhs == "false") { vs.values[name] = "false"; ++vs.set_false_count; return; }
    // integer literal?
    bool is_int = !rhs.empty();
    std::size_t j = (rhs[0] == '-') ? 1 : 0;
    if (j == rhs.size()) is_int = false;
    for (; j < rhs.size(); ++j) if (!std::isdigit((unsigned char)rhs[j])) {
        is_int = false; break;
    }
    if (is_int) { vs.values[name] = rhs; ++vs.set_int_count; return; }
    ++vs.uninterpreted_count;
}

struct StepMutation {
    int          step = 0;
    std::string  location;
    std::vector<std::pair<std::string, std::string>> mutations; // (var, value)
};

struct DispatcherReport {
    int steps_dispatched = 0;
    int triggers_dispatched = 0;
    int effects_applied = 0;
    int effects_uninterpreted = 0;
    VariableState vs;
    std::vector<StepMutation> timeline;     // per-step mutation list
};

static DispatcherReport dispatch_playthrough(const GameDefinition& gd) {
    DispatcherReport rep;
    for (const FlowStep& fs : gd.steps) {
        auto sit = gd.scenes.find(fs.location);
        if (sit == gd.scenes.end()) continue;
        ++rep.steps_dispatched;
        StepMutation sm;
        sm.step = fs.step;
        sm.location = fs.location;
        for (const std::string& tid : sit->second.trigger_ids) {
            auto tix = gd.trigger_by_id.find(tid);
            if (tix == gd.trigger_by_id.end()) continue;
            const TriggerSummary& ts = gd.triggers[tix->second];
            ++rep.triggers_dispatched;
            std::size_t vars_before = rep.vs.values.size();
            int u_before = rep.vs.uninterpreted_count;
            apply_effect(ts.effect_summary, rep.vs);
            if (rep.vs.uninterpreted_count == u_before) ++rep.effects_applied;
            else ++rep.effects_uninterpreted;
            // capture the new mutation (whether the key was new or updated):
            // walk the mutated map and find the var(s) touched by ts.vars_written.
            for (const std::string& vn : ts.vars_written) {
                auto v_it = rep.vs.values.find(vn);
                if (v_it != rep.vs.values.end()) {
                    sm.mutations.emplace_back(vn, v_it->second);
                }
            }
            // fallback: if vars_written was empty, try to detect any net-new var.
            if (ts.vars_written.empty() &&
                rep.vs.values.size() > vars_before) {
                // best-effort: pick the last-inserted key (std::map is ordered;
                // we can't trivially know which was new without a probe). Skip.
            }
        }
        rep.timeline.push_back(std::move(sm));
    }
    return rep;
}

struct StateMachineSim {
    // per-location: which phase index we are at, and which checkpoints have fired.
    struct Loc {
        int phase_index = 0;                            // 0 = first entry in phase_machine
        std::set<std::string> fired;                    // checkpoints that have triggered
        bool advanced_any = false;
    };
    std::map<std::string, Loc> by_location;
    int total_advances = 0;
    int total_completions = 0;
};

// Advance the state machine of `location` by firing one synthetic checkpoint
// per step (round-robin over its declared checkpoints). When all checkpoints
// have fired, the phase index advances; reaching the last phase counts as a
// completion. This is a byte-direct shape simulation — it does not pretend to
// reproduce the game's runtime evaluation, only its declared structure.
static void simulate_step(const ScenePart& sc, StateMachineSim& sim,
                          std::string& diag) {
    if (sc.checkpoints.empty()) return;
    auto& loc = sim.by_location[sc.location];
    const std::string& cp = sc.checkpoints[loc.fired.size() % sc.checkpoints.size()];
    auto ins = loc.fired.insert(cp);
    if (ins.second) {
        diag += "  fired[" + cp + "]";
    }
    if (loc.fired.size() >= sc.checkpoints.size()) {
        // all checkpoints fired -> advance phase
        if (!sc.phase_machine.empty() &&
            loc.phase_index + 1 < (int)sc.phase_machine.size()) {
            ++loc.phase_index;
            loc.advanced_any = true;
            ++sim.total_advances;
            diag += "  advance->" + sc.phase_machine[loc.phase_index];
            loc.fired.clear();    // checkpoints reset for next phase
        } else if (!sc.phase_machine.empty() &&
                   loc.phase_index + 1 == (int)sc.phase_machine.size()) {
            // already at the last phase; treat as completion once.
            if (!loc.advanced_any) {
                loc.advanced_any = true;
                ++sim.total_advances;
            }
            ++sim.total_completions;
            diag += "  complete";
        }
    }
}

static ValidatorTally validate_scripted_flow(const engine::GameModel& /*g*/,
                                             const GameDefinition&    gd,
                                             std::vector<StepReport>& reports,
                                             StateMachineSim&         sim) {
    ValidatorTally tally;
    for (const FlowStep& fs : gd.steps) {
        StepReport r;
        r.step = fs.step;
        r.day = fs.day;
        r.location = fs.location;
        r.action_count = fs.action_count;

        std::ostringstream diag;
        bool triggers_ok = true;

        auto sit = gd.scenes.find(fs.location);
        if (sit != gd.scenes.end()) {
            r.scene_found = true;
            for (const std::string& tid : sit->second.trigger_ids) {
                ++r.triggers_referenced;
                if (gd.trigger_by_id.count(tid)) ++r.triggers_resolved;
                else {
                    diag << " MISSING trigger[" << tid << "]";
                    triggers_ok = false;
                }
            }
        }

        if (fs.action_count == 0) {
            diag << " NO_ACTIONS";
            r.status = StepStatus::Fail;
            tally.fail++;
        } else if (r.scene_found && triggers_ok) {
            r.status = StepStatus::Pass;
            tally.pass++;
            std::string sim_diag;
            simulate_step(sit->second, sim, sim_diag);
            diag << sim_diag;
        } else if (!r.scene_found) {
            diag << " walkthrough-only (no byte-direct scene catalog entry)";
            r.status = StepStatus::WalkthroughOnly;
            tally.walkthrough_only++;
        } else {
            r.status = StepStatus::Fail;
            tally.fail++;
        }
        r.diagnostic = diag.str();
        reports.push_back(r);
    }
    return tally;
}

} // namespace engine
} // namespace xfiles

// ===========================================================================
// main — load, reconstruct, and demonstrate the game loop
// ===========================================================================
//
// CLI surface:
//   xfiles_engine <XFILES.HDB> [XV_DIR]
//   xfiles_engine --validate-flow <game_definition.json> [<XFILES.HDB>]
//
// `--validate-flow` runs the scripted-playthrough validator over the unified
// honest model produced by Build A (game_definition.json). When the HDB path
// is also supplied (positional, after the flag), the validator cross-checks
// references against the HDB-derived GameModel as well; otherwise it walks the
// JSON alone. Exit code is 0 when all node-steps verify, 1 otherwise.
int main(int argc, char** argv) {
    using namespace xfiles;

    // --- minimal flag parser (positional + --validate-flow + --print-trace) ---
    std::string validate_flow_path;
    std::string print_trace_path;
    std::string json_out_path;
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--validate-flow") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--validate-flow requires a path argument\n");
                return 1;
            }
            validate_flow_path = argv[++i];
        } else if (a == "--print-trace") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--print-trace requires a path argument\n");
                return 1;
            }
            print_trace_path = argv[++i];
        } else if (a == "--json-out") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--json-out requires a path argument\n");
                return 1;
            }
            json_out_path = argv[++i];
        } else if (a == "--help" || a == "-h") {
            std::fprintf(stderr,
                "Usage: xfiles_engine <XFILES.HDB> [XV_dir_with_HOT_files]\n"
                "       xfiles_engine --validate-flow <game_definition.json> "
                "[--json-out <path>] [<XFILES.HDB>]\n"
                "       xfiles_engine --print-trace <playthrough_trace.json>\n");
            return 0;
        } else {
            positional.push_back(a);
        }
    }

    // --- --print-trace mode (readable per-step trace summary) ----------------
    if (!print_trace_path.empty()) {
        std::string buf;
        if (!engine::json::load_file(print_trace_path, buf)) {
            std::fprintf(stderr, "cannot read %s\n", print_trace_path.c_str());
            return 1;
        }
        engine::json::Parser parser(buf.data(), buf.size());
        engine::json::Value root;
        if (!parser.parse(root) || !root.is_obj()) {
            std::fprintf(stderr, "cannot parse %s: %s\n",
                         print_trace_path.c_str(), parser.error().c_str());
            return 1;
        }
        auto str_at = [](const engine::json::Value& v,
                          const std::string& key) -> std::string {
            const engine::json::Value& f = v.at(key);
            if (f.is_str()) return f.as_str();
            if (f.is_obj() && f.at("value").is_str()) return f.at("value").as_str();
            return {};
        };
        auto int_at = [](const engine::json::Value& v,
                          const std::string& key) -> long long {
            const engine::json::Value& f = v.at(key);
            if (f.is_num()) return f.as_int();
            if (f.is_obj() && f.at("value").is_num()) return f.at("value").as_int();
            return 0;
        };

        std::printf("=== Playthrough trace ===\n");
        std::printf("source: %s\n", print_trace_path.c_str());
        const engine::json::Value& steps = root.at("steps");
        if (!steps.is_arr()) {
            std::fprintf(stderr, "trace has no steps[]\n");
            return 1;
        }
        for (const engine::json::Value& s : steps.as_arr()) {
            int step = (int)int_at(s, "step");
            std::string day = str_at(s, "day");
            std::string loc = str_at(s, "location");
            std::string status = s.at("status").is_str() ? s.at("status").as_str() : "";
            std::string actions = str_at(s, "actions_walkthrough");
            const engine::json::Value& trigs = s.at("triggers_in_scope");
            const engine::json::Value& convs = s.at("conversations_in_scope");
            const engine::json::Value& dlg   = s.at("dialogue_lines_sample");
            std::size_t n_tr = trigs.is_arr() ? trigs.as_arr().size() : 0;
            std::size_t n_cv = convs.is_arr() ? convs.as_arr().size() : 0;
            std::size_t n_dl = dlg.is_arr()   ? dlg.as_arr().size()   : 0;
            std::printf("\n--- step %2d (%s) %s [%s] ---\n",
                        step, day.c_str(), loc.c_str(), status.c_str());
            if (!actions.empty()) {
                std::printf("  actions: %s\n", actions.c_str());
            }
            std::printf("  triggers: %zu  conversations: %zu  dialogue samples: %zu\n",
                        n_tr, n_cv, n_dl);
            if (n_tr && trigs.as_arr().size() > 0) {
                std::size_t shown = 0;
                for (const engine::json::Value& t : trigs.as_arr()) {
                    if (shown++ >= 3) break;
                    std::printf("    trig: %s -- %s\n",
                                str_at(t, "trigger_id").c_str(),
                                str_at(t, "effect_summary").c_str());
                }
                if (n_tr > 3) std::printf("    (+ %zu more)\n", n_tr - 3);
            }
            if (n_dl && dlg.as_arr().size() > 0) {
                std::size_t shown = 0;
                for (const engine::json::Value& d : dlg.as_arr()) {
                    if (shown++ >= 2) break;
                    std::string text = str_at(d, "text");
                    if (text.size() > 90) text = text.substr(0, 87) + "...";
                    std::printf("    line[%lld]: %s\n",
                                int_at(d, "string_id"), text.c_str());
                }
            }
        }
        std::printf("\n%zu steps printed\n", steps.as_arr().size());
        return 0;
    }

    // --- --validate-flow mode (scripted playthrough validator) ---------------
    if (!validate_flow_path.empty()) {
        engine::GameDefinition gd;
        std::string err;
        if (!engine::load_game_definition(validate_flow_path, gd, err)) {
            std::fprintf(stderr, "cannot load game definition: %s\n", err.c_str());
            return 1;
        }
        std::printf("=== Scripted playthrough validator ===\n");
        std::printf("Loaded %s\n", validate_flow_path.c_str());
        std::printf("  flow.steps             : %zu steps\n", gd.steps.size());
        std::printf("  scenes                 : %zu entries\n", gd.scenes.size());
        std::printf("  triggers               : %zu summaries\n", gd.triggers.size());
        std::printf("  actions_vocabulary     : %zu opcodes\n", gd.actions.size());
        std::printf("  conversations          : %d\n", gd.conversation_count);
        std::printf("  dialogues              : %d\n", gd.dialogue_count);
        std::printf("  variable_count         : %d\n", gd.variable_count);

        // Optionally load the HDB for the soft label cross-check.
        engine::GameModel g;
        if (!positional.empty()) {
            hdb::HDBContainer container;
            if (container.load_from_file(positional[0].c_str())) {
                const auto& raw = container.raw_bytes();
                g = engine::load_game(raw.data(), raw.size());
                std::printf("  HDB cross-check       : %s (%zu bytes, %zu scenes)\n",
                            positional[0].c_str(), raw.size(), g.scenes.size());
            } else {
                std::printf("  HDB cross-check       : SKIPPED (cannot open %s)\n",
                            positional[0].c_str());
            }
        } else {
            std::printf("  HDB cross-check       : SKIPPED (no HDB path supplied)\n");
        }

        std::vector<engine::StepReport> reports;
        engine::StateMachineSim sim;
        engine::ValidatorTally tally =
            engine::validate_scripted_flow(g, gd, reports, sim);
        int total = (int)reports.size();

        auto label = [](engine::StepStatus s) {
            switch (s) {
                case engine::StepStatus::Pass:            return "PASS";
                case engine::StepStatus::WalkthroughOnly: return "WALK";
                case engine::StepStatus::Fail:            return "FAIL";
            }
            return "??";
        };
        std::printf("\nPer-step results:\n");
        for (const engine::StepReport& r : reports) {
            std::string suffix = r.diagnostic.empty() ? std::string()
                                                       : ("  --" + r.diagnostic);
            std::printf("  step %2d  %-20s  triggers %d/%d  actions %d  %s%s\n",
                        r.step, r.location.c_str(),
                        r.triggers_resolved, r.triggers_referenced,
                        r.action_count,
                        label(r.status),
                        suffix.c_str());
        }
        std::printf("\nscripted playthrough: %d/%d byte-direct PASS, "
                    "%d walkthrough-only, %d FAIL\n",
                    tally.pass, total, tally.walkthrough_only, tally.fail);

        // State-machine simulation summary (byte-direct shape walk).
        std::printf("\nState-machine simulation:\n");
        int locs_with_state = 0;
        for (const auto& kv : sim.by_location) {
            const auto& loc = kv.second;
            auto it = gd.scenes.find(kv.first);
            if (it == gd.scenes.end()) continue;
            ++locs_with_state;
            const auto& sc = it->second;
            std::string phase_now = sc.phase_machine.empty()
                ? std::string("-")
                : sc.phase_machine[loc.phase_index];
            std::printf("  %-15s  phase %d/%zu (%s)  checkpoints %zu/%zu\n",
                        kv.first.c_str(),
                        loc.phase_index + 1,
                        sc.phase_machine.empty() ? size_t(1)
                                                  : sc.phase_machine.size(),
                        phase_now.c_str(),
                        loc.fired.size(),
                        sc.checkpoints.size());
        }
        std::printf("  Total phase advances: %d, completions: %d\n",
                    sim.total_advances, sim.total_completions);

        // Headless dispatcher: fire each step's triggers, mutate the variable
        // state map, report the result.
        engine::DispatcherReport disp = engine::dispatch_playthrough(gd);
        std::printf("\nHeadless dispatcher:\n");
        std::printf("  steps dispatched      : %d / %d\n",
                    disp.steps_dispatched, (int)gd.steps.size());
        std::printf("  triggers dispatched   : %d\n", disp.triggers_dispatched);
        std::printf("  effects applied       : %d (true=%d  false=%d  int=%d)\n",
                    disp.effects_applied,
                    disp.vs.set_true_count, disp.vs.set_false_count,
                    disp.vs.set_int_count);
        std::printf("  effects uninterpreted : %d\n", disp.effects_uninterpreted);
        std::printf("  distinct variables set: %zu\n", disp.vs.values.size());

        // Optional machine-readable export of the validator + dispatcher state.
        if (!json_out_path.empty()) {
            std::FILE* f = std::fopen(json_out_path.c_str(), "wb");
            if (!f) {
                std::fprintf(stderr, "cannot write %s\n", json_out_path.c_str());
                return 1;
            }
            auto esc = [](const std::string& s) {
                std::string out; out.reserve(s.size() + 2);
                for (char c : s) {
                    if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); }
                    else if (c == '\n') out += "\\n";
                    else if (c == '\r') out += "\\r";
                    else if (c == '\t') out += "\\t";
                    else if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                        out += buf;
                    }
                    else out.push_back(c);
                }
                return out;
            };
            std::fprintf(f, "{\n");
            std::fprintf(f, "  \"_about\": \"--validate-flow + dispatcher report\",\n");
            std::fprintf(f, "  \"steps\": %d, \"pass\": %d, \"walkthrough_only\": %d, "
                            "\"fail\": %d,\n",
                         total, tally.pass, tally.walkthrough_only, tally.fail);
            std::fprintf(f, "  \"per_step\": [\n");
            for (std::size_t k = 0; k < reports.size(); ++k) {
                const engine::StepReport& r = reports[k];
                const char* st =
                    r.status == engine::StepStatus::Pass ? "pass" :
                    r.status == engine::StepStatus::WalkthroughOnly ? "walkthrough-only" :
                    "fail";
                std::fprintf(f, "    {\"step\": %d, \"day\": \"%s\", "
                                "\"location\": \"%s\", \"status\": \"%s\", "
                                "\"triggers_referenced\": %d, "
                                "\"triggers_resolved\": %d, "
                                "\"action_count\": %d}%s\n",
                             r.step, esc(r.day).c_str(), esc(r.location).c_str(),
                             st, r.triggers_referenced, r.triggers_resolved,
                             r.action_count,
                             (k + 1 < reports.size()) ? "," : "");
            }
            std::fprintf(f, "  ],\n");
            std::fprintf(f, "  \"state_machine\": {\n");
            std::size_t i_loc = 0;
            for (const auto& kv : sim.by_location) {
                auto sit = gd.scenes.find(kv.first);
                const auto& sc = sit->second;
                std::string phase_now = sc.phase_machine.empty() ? std::string("-")
                                          : sc.phase_machine[kv.second.phase_index];
                std::fprintf(f, "    \"%s\": {\"phase_index\": %d, "
                                "\"phase_count\": %zu, \"phase_now\": \"%s\", "
                                "\"checkpoints_fired\": %zu, "
                                "\"checkpoints_total\": %zu}%s\n",
                             esc(kv.first).c_str(),
                             kv.second.phase_index + 1,
                             sc.phase_machine.empty() ? size_t(1) : sc.phase_machine.size(),
                             esc(phase_now).c_str(),
                             kv.second.fired.size(),
                             sc.checkpoints.size(),
                             (++i_loc < sim.by_location.size()) ? "," : "");
            }
            std::fprintf(f, "  },\n");
            std::fprintf(f, "  \"dispatcher\": {\n");
            std::fprintf(f, "    \"steps_dispatched\": %d,\n", disp.steps_dispatched);
            std::fprintf(f, "    \"triggers_dispatched\": %d,\n", disp.triggers_dispatched);
            std::fprintf(f, "    \"effects_applied\": %d,\n", disp.effects_applied);
            std::fprintf(f, "    \"effects_uninterpreted\": %d,\n", disp.effects_uninterpreted);
            std::fprintf(f, "    \"variables\": {\n");
            std::size_t i_v = 0;
            for (const auto& kv : disp.vs.values) {
                std::fprintf(f, "      \"%s\": \"%s\"%s\n",
                             esc(kv.first).c_str(), esc(kv.second).c_str(),
                             (++i_v < disp.vs.values.size()) ? "," : "");
            }
            std::fprintf(f, "    },\n");
            // Per-step timeline of mutations.
            std::fprintf(f, "    \"timeline\": [\n");
            for (std::size_t ti = 0; ti < disp.timeline.size(); ++ti) {
                const engine::StepMutation& sm = disp.timeline[ti];
                std::fprintf(f, "      {\"step\": %d, \"location\": \"%s\", "
                                "\"mutations\": [", sm.step,
                             esc(sm.location).c_str());
                for (std::size_t mi = 0; mi < sm.mutations.size(); ++mi) {
                    std::fprintf(f, "{\"var\": \"%s\", \"value\": \"%s\"}%s",
                                 esc(sm.mutations[mi].first).c_str(),
                                 esc(sm.mutations[mi].second).c_str(),
                                 (mi + 1 < sm.mutations.size()) ? ", " : "");
                }
                std::fprintf(f, "]}%s\n",
                             (ti + 1 < disp.timeline.size()) ? "," : "");
            }
            std::fprintf(f, "    ]\n");
            std::fprintf(f, "  }\n");
            std::fprintf(f, "}\n");
            std::fclose(f);
            std::printf("\nWrote %s\n", json_out_path.c_str());
        }
        return (tally.fail == 0 && total > 0) ? 0 : 1;
    }

    // --- legacy positional mode ---------------------------------------------
    if (positional.empty()) {
        std::fprintf(stderr,
            "Usage: xfiles_engine <XFILES.HDB> [XV_dir_with_HOT_files]\n"
            "       xfiles_engine --validate-flow <game_definition.json> "
            "[<XFILES.HDB>]\n");
        return 1;
    }
    const char* hdb_path = positional[0].c_str();

    hdb::HDBContainer container;
    if (!container.load_from_file(hdb_path)) {
        std::fprintf(stderr, "cannot open %s\n", hdb_path);
        return 2;
    }
    const auto& raw = container.raw_bytes();

    // 1) load the game model from the HDB
    engine::GameModel g = engine::load_game(raw.data(), raw.size());

    std::printf("=== The X-Files — reconstructed from %s (%zu bytes) ===\n\n",
                hdb_path, raw.size());

    // 2) class inventory
    std::printf("Object classes present (class_id  name  count):\n");
    for (const auto& kv : g.by_class) {
        std::printf("  0x%02X  %-16s %5zu\n",
                    kv.first, engine::class_name(kv.first), kv.second.size());
    }

    // 3) story flow: the ordered scene-moments
    std::printf("\nStory flow — %zu scene-moments (Node / Location):\n",
                g.scenes.size());
    for (const auto& kv : g.scenes) {
        std::printf("  %-10s : %zu records\n", kv.first.c_str(), kv.second.size());
    }

    // 4) triggers: the event wiring
    std::size_t with_rel = 0;
    for (const auto& t : g.triggers) if (!t.relations.empty()) ++with_rel;
    std::printf("\nTriggers: %zu  (%zu carry binary-relation conditions)\n",
                g.triggers.size(), with_rel);
    int shown = 0;
    for (const auto& t : g.triggers) {
        if (t.relations.empty() && t.affects.empty()) continue;
        std::printf("  trigger @0x%zx:", t.offset);
        for (const auto& r : t.relations)
            std::printf(" [%s op(0x%02x) %s]",
                        engine::class_name(r.class_a), r.relation,
                        engine::class_name(r.class_b));
        for (const auto& a : t.affects) std::printf(" -> %s", a.c_str());
        std::printf("\n");
        if (++shown >= 8) { std::printf("  ...\n"); break; }
    }
    std::printf("Variables: %d   Actions: %d\n", g.variable_count, g.action_count);

    // 5) hotspots: load every XV/<scene>.HOT, then demonstrate the per-scene
    //    game loop (hit-test the cursor against the rects).
    if (positional.size() >= 2) {
        const char* xv_dir = positional[1].c_str();
        namespace fs = std::filesystem;
        std::error_code ec;
        std::size_t total = 0;
        for (auto it = fs::directory_iterator(xv_dir, ec);
             !ec && it != fs::directory_iterator(); ++it) {
            const fs::path& p = it->path();
            std::string ext = p.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
            if (ext != ".HOT") continue;
            std::ifstream f(p, std::ios::binary);
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                       std::istreambuf_iterator<char>());
            auto hs = engine::load_hot_file(bytes);
            if (!hs.empty()) { g.hotspots[p.stem().string()] = hs; total += hs.size(); }
        }
        std::printf("\nHotspots loaded from %s : %zu scenes, %zu clickable "
                    "rects.\n", xv_dir, g.hotspots.size(), total);

        // demonstrate the game loop on the first scene with hotspots
        if (!g.hotspots.empty()) {
            const auto& kv = *g.hotspots.begin();
            const auto& hs = kv.second;
            int cx = (hs[0].x_min + hs[0].x_max) / 2;
            int cy = (hs[0].y_min + hs[0].y_max) / 2;
            const engine::Hotspot* h = engine::hit_test(hs, cx, cy);
            std::printf("Game loop demo — scene %s: cursor at (%d,%d) hits ",
                        kv.first.c_str(), cx, cy);
            if (h)
                std::printf("hotspot #%d rect(%d,%d)-(%d,%d) -> dispatch "
                            "action_id %u\n", h->index, h->x_min, h->y_min,
                            h->x_max, h->y_max, h->action_id_1);
            else
                std::printf("no hotspot\n");
        }
    } else {
        std::printf("\n(Pass the XV/ directory as arg 2 to load hotspot "
                    "geometry and run the hit-test loop.)\n");
    }
    return 0;
}
