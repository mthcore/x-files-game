// SPDX-License-Identifier: MIT
// NL system class bootstrap — registers the captured-live CNeo* classes.

#include "nl/system_classes.h"

namespace xfiles {
namespace nl {

namespace {
// Stub factory functions for system classes. The reference implementation doesn't allocate
// real NL objects yet ; future sessions will wire actual factories from
// `the on-disk Read sequence` (CNeoIDList) etc. The factory pointer
// is captured here as a placeholder for byte-direct call dispatch.
void* factory_stub(void* /*heap*/, uint32_t /*init_flag*/) { return nullptr; }
}  // namespace

void register_nl_system_classes() {
    auto& reg = ClassRegistry::instance();

    // system class: root abstract base
    ClassMeta* persist = reg.register_class(
        SystemClassId::CNeoPersist, ClassKind::System,
        "CNeoPersist", &factory_stub, /*parent=*/nullptr,
        /*default_value=*/0u);

    // system class: partition manager (children of CNeoPersist)
    reg.register_class(
        SystemClassId::CNeoPartMgr, ClassKind::System,
        "CNeoPartMgr", &factory_stub, persist, 0u);

    // system class: long-index B-tree walker
    reg.register_class(
        SystemClassId::CNeoLongIndex, ClassKind::System,
        "CNeoLongIndex", &factory_stub, persist, 0u);

    // system class: filesystem abstraction
    reg.register_class(
        SystemClassId::CNeoFileLocation, ClassKind::System,
        "CNeoFileLocation", &factory_stub, persist, 0u);

    // system class: ID-list container (carries 'ID  ' fourcc )
    reg.register_class(
        SystemClassId::CNeoIDList, ClassKind::System,
        "CNeoIDList", &factory_stub, persist,
        /*default_value=*/0x49442020u);  // 'ID  '
}

namespace {
// 51 VC* user-class entries — sourced from the format notes.
// {class_id, name, factory_va}. The factory_va is the factory id ;
// callers can map to reference-implementation factories via the name string.
struct VCEntry { uint32_t class_id; const char* name; uint32_t xfiles_factory_va; };
constexpr VCEntry k_vc_entries[] = {
    {0x27, "VCTitle",               },
    {0x28, "VCNode",                },
    {0x29, "VCLocaton",             },
    {0x2a, "VCViewPoint",           },
    {0x2b, "VCView",                },
    {0x2c, "VCCharacter",           },
    {0x2d, "VCCharView",            },
    {0x2e, "VCCharViewList",        },
    {0x2f, "VCName",                },
    {0x31, "VCConversation",        },
    {0x32, "VCConversationList",    },
    {0x33, "VCNav",                 },
    {0x34, "VCNav_List",            },
    {0x35, "VCAssetRef",            },
    {0x36, "VCAssetRefList",        },
    {0x37, "VCExplorable",          },
    {0x38, "VCExplorableList",      },
    {0x39, "VCHotSpot",             },
    {0x3a, "VCHotSpotList",         },
    {0x3b, "VCStdHotSpotList",      },
    {0x3c, "VCAssetCategory",       },
    {0x3d, "VCCursor",              },
    {0x3e, "VCDefaultCursors",      },
    {0x40, "VCDefaultHotSpots",     },
    {0x41, "VCAction",              },
    {0x42, "VCActionList",          },
    {0x43, "VCBlob",                },
    {0x44, "VCPlayer",              },
    {0x46, "VCEnabled",             },
    {0x47, "VCActionIcon",          },
    {0x48, "VCEmotionIcon",         },
    {0x49, "VCEvidenceIcon",        },
    {0x4a, "VCIdeaIcon",            },
    {0x4b, "VCInventory",           },
    {0x4c, "VCIdeaMap",             },
    {0x4d, "VCIdeaMapList",         },
    {0x4e, "VCIFaceLayout",         },
    {0x4f, "VCInterfaceItem",       },
    {0x50, "VCString",              },
    {0x51, "VCTrigger",             },
    {0x52, "VCTriggerList",         },
    {0x53, "VCVariable",            },
    {0x54, "VCStdAction",           },
    {0x55, "VCStdActionList",       },
    {0x56, "VCConversationHistory", },
    {0x57, "VCGameState",           },
    {0x58, "VCPDANotes",            },
    {0x59, "VCPhoto",               },
    {0x5a, "VCEmail",               },
    {0x5b, "VCEmailRead",           },
    {0x5c, "VCEmailPending",        },
};
}  // namespace

void register_vc_user_classes() {
    auto& reg = ClassRegistry::instance();
    // Per user-class branch (param_2 == 0), parent defaults to
    // walking via factory chain. We can't walk x86 factory from here, so
    // we leave parent=nullptr and document : future sessions can wire the
    // hierarchy via the format notes.
    for (const auto& e : k_vc_entries) {
        reg.register_class(e.class_id, ClassKind::User, e.name,
                            /*factory=*/&factory_stub,
                            /*parent=*/nullptr,
                            /*default_value=*/0u);
    }
}

}  // namespace nl
}  // namespace xfiles
