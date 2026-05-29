// SPDX-License-Identifier: MIT
// HyperBole NeoLogic subclass decoders (class_id 0x0F-0x1E).
//
// These are HyperBole Studios's application-specific NeoLogic subclasses
// — they sit BELOW the standard kNeo*ID range (0x00-0x0B) and ABOVE
// the 0x27+ VC* application classes. They implement the script engine
// substrate : conditions, actions, scopes, references.
//
// Format inferred from byte-direct pattern analysis (L9 in hdb_extract).
// All "16B reference" records share the same structure :
//
//   struct HB_Reference16 {
//       uint16_t ref_addr_BE;    // bytes 0-1 BE - source/page address
//       uint8_t  pad_2_4[3];      // always 0
//       uint8_t  target_class;    // byte 5 - class of referenced object
//       uint8_t  flags;           // byte 6 - 0x00, 0x1c, or 0x80
//       uint8_t  pad_7;           // always 0
//       uint32_t second_ref_BE;  // bytes 8-11 BE - secondary link
//       uint32_t data_id_BE;     // bytes 12-15 BE - value/ID
//   };
//
// 9-byte variant : same as above truncated after byte 8 (no second_ref,
// no data_id) — represents a shorter reference.
//
// Larger variants (24B, 40B, 80B, 144B, 160B) contain inline data after
// the 16B header (path strings, name strings, lists, etc.)

#ifndef NL_HB_SUBRECORDS_H
#define NL_HB_SUBRECORDS_H

#include "nl/neo_stream.h"

#include <cstdint>
#include <string>
#include <vector>

namespace xfiles {
namespace nl {

/// 16-byte reference record (the common header of all HB classes).
struct HBReference16 {
    uint16_t ref_addr;         // BE
    uint8_t  target_class_id;
    uint8_t  flags;             // 0x00, 0x1c, 0x80
    uint32_t second_ref;        // BE
    uint32_t data_id;            // BE

    static HBReference16 read(const uint8_t* p, std::size_t size) {
        HBReference16 r{};
        if (size < 16) return r;
        r.ref_addr        = (static_cast<uint16_t>(p[0]) << 8) | p[1];
        r.target_class_id = p[5];
        r.flags           = p[6];
        r.second_ref      = (static_cast<uint32_t>(p[8])  << 24)
                          | (static_cast<uint32_t>(p[9])  << 16)
                          | (static_cast<uint32_t>(p[10]) << 8)
                          |                       p[11];
        r.data_id         = (static_cast<uint32_t>(p[12]) << 24)
                          | (static_cast<uint32_t>(p[13]) << 16)
                          | (static_cast<uint32_t>(p[14]) << 8)
                          |                       p[15];
        return r;
    }
};

/// HBExpression (class 0x14) - a condition reference.
/// 16-byte structure encoding LHS op RHS.
///
/// Inferred mapping from byte-direct analysis (patterns + flags observed):
///   ref_addr        = "expression key" or local id within parent trigger
///   target_class    = class of LHS operand (typically VCVariable 0x53,
///                     VCGameState 0x57, or HyperBole internal class)
///   flags           = operator + variant :
///                     0x00 : default Eq (probably "= TRUE")
///                     0x1c : non-trivial comparison (Gt/Lt/Ge/Le)
///                     0x80 : NotEq (high bit = negate)
///   second_ref      = LHS variable address / index
///   data_id         = RHS literal value OR variable id
///
/// Full semantics require the format notes of HBExpression::evaluate in
/// the on-disk format ; for now this structural decoder produces a usable form.
enum class HBExprOp : uint8_t {
    Eq, NotEq, Gt, Lt, Le, Ge, Unknown
};

inline const char* hb_op_name(HBExprOp o) {
    switch (o) {
        case HBExprOp::Eq:    return "=";
        case HBExprOp::NotEq: return "!=";
        case HBExprOp::Gt:    return ">";
        case HBExprOp::Lt:    return "<";
        case HBExprOp::Le:    return "<=";
        case HBExprOp::Ge:    return ">=";
        case HBExprOp::Unknown: break;
    }
    return "?";
}

struct HBExpression {
    HBReference16 ref;
    HBExprOp      op;
    uint16_t      lhs_var_id;     // = ref.ref_addr (= LHS variable identifier)
    uint8_t       lhs_class_id;    // = ref.target_class (= class of LHS object)
    uint32_t      rhs_value;       // = ref.data_id (= RHS literal or RHS var id)
    uint32_t      rhs_ref;         // = ref.second_ref (= RHS reference)

    static HBExpression decode(const uint8_t* payload, std::size_t size) {
        HBExpression e{};
        e.ref = HBReference16::read(payload, size);
        e.lhs_var_id   = e.ref.ref_addr;
        e.lhs_class_id = e.ref.target_class_id;
        e.rhs_value    = e.ref.data_id;
        e.rhs_ref      = e.ref.second_ref;

        // Decode operator from flags byte
        switch (e.ref.flags) {
            case 0x00: e.op = HBExprOp::Eq;    break;
            case 0x80: e.op = HBExprOp::NotEq; break;
            case 0x1c: e.op = HBExprOp::Gt;    break;
            // Other flag values seen empirically — fall through
            default:   e.op = HBExprOp::Unknown; break;
        }
        return e;
    }

    /// Format for human reading : "0x53.0x1234 = 0x91d"
    std::string to_string() const {
        char buf[80];
        std::snprintf(buf, sizeof(buf),
                      "cls_0x%02x.id_0x%04x %s 0x%x",
                      lhs_class_id, lhs_var_id,
                      hb_op_name(op), rhs_value);
        return buf;
    }

    /// Evaluate the expression with an explicit LHS value supplied by the
    /// caller (e.g., looked up via game_vars by lhs_var_id).
    ///
    /// Returns the evaluated boolean : `lhs_value op rhs_value`.
    /// Falls back to `true` only if op is Unknown (= fail-open, so
    /// conditions we can't decode don't block the trigger).
    bool evaluate(uint32_t lhs_value) const {
        switch (op) {
            case HBExprOp::Eq:      return lhs_value == rhs_value;
            case HBExprOp::NotEq:   return lhs_value != rhs_value;
            case HBExprOp::Gt:
                return static_cast<int32_t>(lhs_value) >
                       static_cast<int32_t>(rhs_value);
            case HBExprOp::Lt:
                return static_cast<int32_t>(lhs_value) <
                       static_cast<int32_t>(rhs_value);
            case HBExprOp::Le:
                return static_cast<int32_t>(lhs_value) <=
                       static_cast<int32_t>(rhs_value);
            case HBExprOp::Ge:
                return static_cast<int32_t>(lhs_value) >=
                       static_cast<int32_t>(rhs_value);
            case HBExprOp::Unknown:
            default:
                return true;  // fail-open
        }
    }

    /// Evaluate via a value-lookup callback. Caller maps
    /// (lhs_class_id, lhs_var_id) → uint32_t (e.g., game_vars).
    /// If the callback returns false, fail-open returns true.
    template <typename Lookup>
    bool evaluate_with(Lookup lookup_lhs) const {
        uint32_t lhs_value = 0;
        bool ok = lookup_lhs(lhs_class_id, lhs_var_id, lhs_value);
        if (!ok) return true;   // fail-open if we can't resolve
        return evaluate(lhs_value);
    }

    /// Default "no var store available" → fail-open = true.
    bool evaluate_default() const { return true; }
};

/// HBActionBody (class 0x1d) - an action body reference.
/// Action exec : run_action(target_class, data_id, params...).
struct HBActionBody {
    HBReference16 ref;

    static HBActionBody decode(const uint8_t* payload, std::size_t size) {
        return {HBReference16::read(payload, size)};
    }
};

/// HBAssetRef (class 0x15) - asset reference (often with inline path).
struct HBAssetRef {
    HBReference16 ref;
    std::string   inline_path;   // extracted from larger payloads

    static HBAssetRef decode(const uint8_t* payload, std::size_t size) {
        HBAssetRef r;
        r.ref = HBReference16::read(payload, size);
        // Larger payloads may contain inline TLV paths
        if (size > 16) {
            r.inline_path = extract_first_path(payload + 16, size - 16);
        }
        return r;
    }

    /// Extract first ASCII path-like string (4+ printable chars).
    static std::string extract_first_path(const uint8_t* p, std::size_t n) {
        std::string cur;
        std::string best;
        for (std::size_t i = 0; i < n; ++i) {
            uint8_t b = p[i];
            if (b >= 32 && b < 127) {
                cur.push_back(static_cast<char>(b));
            } else {
                if (cur.size() >= 6 &&
                    (cur.find('/') != std::string::npos ||
                     cur.find('\\') != std::string::npos ||
                     cur.find(':')  != std::string::npos)) {
                    return cur;   // first path-like wins
                }
                if (cur.size() > best.size()) best = cur;
                cur.clear();
            }
        }
        if (cur.size() >= 6) return cur;
        return best;
    }
};

/// HBStringTag (class 0x1c) - string/asset tag reference.
struct HBStringTag {
    HBReference16 ref;

    static HBStringTag decode(const uint8_t* payload, std::size_t size) {
        return {HBReference16::read(payload, size)};
    }
};

/// HBListNode (class 0x0f) - linked-list node.
struct HBListNode {
    HBReference16 ref;
    uint32_t      next_link;   // typically the secondary_ref

    static HBListNode decode(const uint8_t* payload, std::size_t size) {
        HBListNode r;
        r.ref       = HBReference16::read(payload, size);
        r.next_link = r.ref.second_ref;
        return r;
    }
};

/// HBSubScope (class 0x11) - scope filter (Title/Node/Location/etc.)
struct HBSubScope {
    HBReference16 ref;
    uint8_t       scope_byte;   // first byte = scope kind

    static HBSubScope decode(const uint8_t* payload, std::size_t size) {
        HBSubScope r;
        r.ref = HBReference16::read(payload, size);
        r.scope_byte = size > 0 ? payload[0] : 0;
        return r;
    }
};

/// HBTargetSpec (class 0x1e) - target specification (4 scope bytes).
struct HBTargetSpec {
    uint8_t scope_bytes[4];
    HBReference16 ref;

    static HBTargetSpec decode(const uint8_t* payload, std::size_t size) {
        HBTargetSpec r{};
        if (size >= 4) {
            r.scope_bytes[0] = payload[0];
            r.scope_bytes[1] = payload[1];
            r.scope_bytes[2] = payload[2];
            r.scope_bytes[3] = payload[3];
        }
        if (size >= 16) r.ref = HBReference16::read(payload, size);
        return r;
    }
};

/// Convert a class_id to human-readable name (HB + standard).
inline const char* class_name(uint16_t cid) {
    switch (cid) {
        case 0x00: return "kNeoNullClass";
        case 0x01: return "CNeoPersist";
        case 0x02: return "CNeoBlob";
        case 0x03: return "CNeoClass";
        case 0x04: return "CNeoSubclass";
        case 0x05: return "CNeoFreeList";
        case 0x06: return "CNeoInode";
        case 0x07: return "CNeoIDIndex";
        case 0x08: return "CNeoParentIndex";
        case 0x09: return "CNeoGenericIndex";
        case 0x0A: return "CNeoPartMgr";
        case 0x0B: return "CNeoIDList";

        case 0x0F: return "HBListNode";
        case 0x10: return "HBStringRef";
        case 0x11: return "HBSubScope";
        case 0x12: return "HBToken";
        case 0x13: return "HBLiteral";
        case 0x14: return "HBExpression";
        case 0x15: return "HBAssetRef";
        case 0x16: return "HBTagRef";
        case 0x17: return "HBRawTag";
        case 0x18: return "HBGenericRef";
        case 0x1B: return "HBActionItem";
        case 0x1C: return "HBStringTag";
        case 0x1D: return "HBActionBody";
        case 0x1E: return "HBTargetSpec";

        case 0x27: return "VCTitle";
        case 0x28: return "VCNode";
        case 0x29: return "VCLocaton";
        case 0x2A: return "VCViewPoint";
        case 0x2B: return "VCView";
        case 0x2C: return "VCCharacter";
        case 0x2D: return "VCCharView";
        case 0x30: return "VCName";
        case 0x31: return "VCConversation";
        case 0x33: return "VCNav";
        case 0x34: return "VCNavList";
        case 0x35: return "VCAssetRef";
        case 0x39: return "VCHotSpot";
        case 0x3D: return "VCCursor";
        case 0x41: return "VCAction";
        case 0x43: return "VCBlob";
        case 0x44: return "VCPlayer";
        case 0x45: return "VCEnabled";
        case 0x4E: return "VCIFaceLayout";
        case 0x50: return "VCString";
        case 0x51: return "VCTrigger";
        case 0x52: return "VCTriggerList";
        case 0x53: return "VCVariable";
        case 0x54: return "VCStdAction";
        case 0x57: return "VCGameState";
        case 0x5B: return "VCEmailRead";

        default:   return "Unknown";
    }
}

}  // namespace nl
}  // namespace xfiles

#endif  // NL_HB_SUBRECORDS_H
