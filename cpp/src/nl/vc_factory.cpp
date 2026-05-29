// SPDX-License-Identifier: MIT
// NL VCFactory implementation.

#include "nl/vc_factory.h"

#include "nl/hdb_serializer.h"
#include "nl/classes/vc_action.h"
#include "nl/classes/vc_action_list.h"
#include "nl/classes/vc_atomic_family.h"
#include "nl/classes/vc_blob.h"
#include "nl/classes/vc_character_family.h"
#include "nl/classes/vc_conversation.h"
#include "nl/classes/vc_email_family.h"
#include "nl/classes/vc_game_state.h"
#include "nl/classes/vc_hotspot_family.h"
#include "nl/classes/vc_icon_family.h"
#include "nl/classes/vc_list_family.h"
#include "nl/classes/vc_nav.h"
#include "nl/classes/vc_remaining.h"
#include "nl/classes/vc_scene_graph.h"
#include "nl/classes/vc_ui_family.h"

#include <cstdio>

namespace xfiles {
namespace nl {

std::unique_ptr<VCObject> make_vc_object(uint32_t class_id) {
    switch (class_id) {
        // Canonical class_ids from the format notes
        case 0x27: return std::unique_ptr<VCObject>(new VCTitle());
        case 0x28: return std::unique_ptr<VCObject>(new VCNode());
        case 0x29: return std::unique_ptr<VCObject>(new VCLocaton());
        case 0x2a: return std::unique_ptr<VCObject>(new VCViewPoint());
        case 0x2b: return std::unique_ptr<VCObject>(new VCView());
        case 0x2c: return std::unique_ptr<VCObject>(new VCCharacter());
        case 0x2d: return std::unique_ptr<VCObject>(new VCCharView());
        case 0x2e: return std::unique_ptr<VCObject>(new VCCharViewList());
        case 0x2f: return std::unique_ptr<VCObject>(new VCName());
        case 0x31: return std::unique_ptr<VCObject>(new VCConversation());
        case 0x32: return std::unique_ptr<VCObject>(new VCConversationList());
        case 0x33: return std::unique_ptr<VCObject>(new VCNav());
        case 0x34: return std::unique_ptr<VCObject>(new VCNav_List());
        case 0x35: return std::unique_ptr<VCObject>(new VCAssetRef());
        case 0x36: return std::unique_ptr<VCObject>(new VCAssetRefList());
        case 0x37: return std::unique_ptr<VCObject>(new VCExplorable());
        case 0x38: return std::unique_ptr<VCObject>(new VCExplorableList());
        case 0x39: return std::unique_ptr<VCObject>(new VCHotSpot());
        case 0x3a: return std::unique_ptr<VCObject>(new VCHotSpotList());
        case 0x3b: return std::unique_ptr<VCObject>(new VCStdHotSpotList());
        case 0x3c: return std::unique_ptr<VCObject>(new VCAssetCategory());
        case 0x3d: return std::unique_ptr<VCObject>(new VCCursor());
        case 0x3e: return std::unique_ptr<VCObject>(new VCDefaultCursors());
        case 0x40: return std::unique_ptr<VCObject>(new VCDefaultHotSpots());
        case 0x41: return std::unique_ptr<VCObject>(new VCAction());
        case 0x42: return std::unique_ptr<VCObject>(new VCActionList());
        case 0x43: return std::unique_ptr<VCObject>(new VCBlob());
        case 0x44: return std::unique_ptr<VCObject>(new VCPlayer());
        case 0x46: return std::unique_ptr<VCObject>(new VCEnabled());
        case 0x47: return std::unique_ptr<VCObject>(new VCActionIcon());
        case 0x48: return std::unique_ptr<VCObject>(new VCEmotionIcon());
        case 0x49: return std::unique_ptr<VCObject>(new VCEvidenceIcon());
        case 0x4a: return std::unique_ptr<VCObject>(new VCIdeaIcon());
        case 0x4b: return std::unique_ptr<VCObject>(new VCInventory());
        case 0x4c: return std::unique_ptr<VCObject>(new VCIdeaMap());
        case 0x4d: return std::unique_ptr<VCObject>(new VCIdeaMapList());
        case 0x4e: return std::unique_ptr<VCObject>(new VCIFaceLayout());
        case 0x4f: return std::unique_ptr<VCObject>(new VCInterfaceItem());
        case 0x50: return std::unique_ptr<VCObject>(new VCString());
        case 0x51: return std::unique_ptr<VCObject>(new VCTrigger());
        case 0x52: return std::unique_ptr<VCObject>(new VCTriggerList());
        case 0x53: return std::unique_ptr<VCObject>(new VCVariable());
        case 0x54: return std::unique_ptr<VCObject>(new VCStdAction());
        case 0x55: return std::unique_ptr<VCObject>(new VCStdActionList());
        case 0x56: return std::unique_ptr<VCObject>(new VCConversationHistory());
        case 0x57: return std::unique_ptr<VCObject>(new VCGameState());
        case 0x58: return std::unique_ptr<VCObject>(new VCPDANotes());
        case 0x59: return std::unique_ptr<VCObject>(new VCPhoto());
        case 0x5a: return std::unique_ptr<VCObject>(new VCEmail());
        case 0x5b: return std::unique_ptr<VCObject>(new VCEmailRead());
        case 0x5c: return std::unique_ptr<VCObject>(new VCEmailPending());
        default:
            return nullptr;
    }
}

bool is_class_supported(uint32_t class_id) {
    switch (class_id) {
        case 0x27:
        case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c:
        case 0x2d: case 0x2e: case 0x2f: case 0x31: case 0x32: case 0x33: case 0x34:
        case 0x35: case 0x36: case 0x37: case 0x38: case 0x39:
        case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x40:
        case 0x41: case 0x42: case 0x43: case 0x44:
        case 0x46: case 0x47: case 0x48: case 0x49: case 0x4a:
        case 0x4b: case 0x4c: case 0x4d:
        case 0x4e: case 0x4f:
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55:
        case 0x56: case 0x57: case 0x58: case 0x59:
        case 0x5a: case 0x5b: case 0x5c:
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// class_id_of — dynamic_cast chain over all supported VC* types. Returns
// the kClassId of the dynamic type or 0 if unsupported.
// ---------------------------------------------------------------------------
uint32_t class_id_of(const VCObject* obj) {
    if (!obj) return 0;
    // Try each supported type. Ordered roughly by frequency of use.
    #define TRY(name) if (dynamic_cast<const name*>(obj)) return name::kClassId
    TRY(VCAction);
    TRY(VCActionList);
    TRY(VCNav);
    TRY(VCConversation);
    TRY(VCInventory);
    TRY(VCActionIcon);
    TRY(VCEvidenceIcon);
    TRY(VCEmotionIcon);
    TRY(VCIdeaIcon);
    TRY(VCBlob);
    TRY(VCAssetRef);
    TRY(VCExplorable);
    TRY(VCHotSpot);
    TRY(VCEmail);
    TRY(VCEmailRead);
    TRY(VCEmailPending);
    TRY(VCConversationHistory);
    TRY(VCPDANotes);
    TRY(VCCharViewList);
    TRY(VCConversationList);
    TRY(VCNav_List);
    TRY(VCAssetRefList);
    TRY(VCExplorableList);
    TRY(VCHotSpotList);
    TRY(VCStdHotSpotList);
    TRY(VCIdeaMapList);
    TRY(VCTriggerList);
    TRY(VCStdActionList);
    TRY(VCNode);
    TRY(VCLocaton);
    TRY(VCView);
    TRY(VCViewPoint);
    TRY(VCCharacter);
    TRY(VCCharView);
    TRY(VCAssetCategory);
    TRY(VCEnabled);
    TRY(VCStdAction);
    TRY(VCString);
    TRY(VCPlayer);
    TRY(VCIdeaMap);
    TRY(VCIFaceLayout);
    TRY(VCInterfaceItem);
    TRY(VCTitle);
    TRY(VCDefaultCursors);
    TRY(VCDefaultHotSpots);
    TRY(VCTrigger);
    TRY(VCName);
    TRY(VCCursor);
    TRY(VCVariable);
    TRY(VCGameState);
    TRY(VCPhoto);
    #undef TRY
    return 0;
}

void write_obj(HDBSerializer* ser, uint32_t class_id_marker,
               const VCObject* obj) {
    if (!ser) return;
    uint32_t cid = class_id_of(obj);
    ser->write_dword_fourcc(class_id_marker, cid);
    if (obj) obj->Write(ser);
}

void write_obj_list(HDBSerializer* ser,
                    uint32_t count_marker,
                    uint32_t class_id_marker,
                    const std::vector<std::unique_ptr<VCObject>>& items) {
    if (!ser) return;
    ser->write_dword_fourcc(count_marker,
                            static_cast<uint32_t>(items.size()));
    for (const auto& it : items) {
        write_obj(ser, class_id_marker, it.get());
    }
}

std::vector<std::unique_ptr<VCObject>> read_obj_list(HDBSerializer* ser,
                                                       uint32_t count_marker,
                                                       uint32_t class_id_marker) {
    std::vector<std::unique_ptr<VCObject>> out;
    if (!ser || ser->error()) return out;

    uint32_t count = ser->read_dword_fourcc(count_marker);
    if (ser->error()) return out;
    if (count > 65536u) {  // sanity cap
        std::fprintf(stderr,
                     "nl::read_obj_list : count %u too large, aborting\n",
                     unsigned(count));
        return out;
    }

    out.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        auto obj = read_obj(ser, class_id_marker);
        if (!obj) {
            // Either unsupported class_id or stream error.
            if (ser->error()) break;
            // unsupported : record nullptr and continue ? For safety break,
            // because we can't seek past the unknown record's bytes.
            std::fprintf(stderr,
                         "nl::read_obj_list : stopping at item %u (unsupported "
                         "or error)\n", unsigned(i));
            break;
        }
        out.push_back(std::move(obj));
    }
    return out;
}

std::unique_ptr<VCObject> read_obj(HDBSerializer* ser,
                                    uint32_t class_id_marker) {
    if (!ser || ser->error()) return nullptr;

    // Read the class_id from the stream (FOURCC-tagged dword).
    uint32_t class_id = ser->read_dword_fourcc(class_id_marker);
    if (ser->error()) return nullptr;

    auto obj = make_vc_object(class_id);
    if (!obj) {
        std::fprintf(stderr,
                     "nl::read_obj : class_id 0x%02x not supported "
                     "(port pending)\n",
                     unsigned(class_id));
        return nullptr;
    }

    // Save/restore the per-record version. The on-disk format keeps version
    // in a context pointer that's per-record ; here we save the outer
    // version, let the inner VCObject::Read update the serializer to
    // reflect THIS record's vers, then restore on return so the outer
    // record's subclass Read continues at its own version.
    uint32_t saved_version = ser->current_version();
    obj->Read(ser);
    // After obj->Read, the serializer's current_version is the inner's
    // vers — restore the outer's so the parent's Read continues correctly.
    if (auto* mem = dynamic_cast<InMemoryHDBSerializer*>(ser)) {
        mem->set_current_version(saved_version);
    }
    return obj;
}

}  // namespace nl
}  // namespace xfiles
