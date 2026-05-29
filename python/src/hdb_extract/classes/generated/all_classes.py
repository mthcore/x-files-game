"""ClassSpec data for all 51 user-level VC* classes.

Each entry mirrors the corresponding `VC*::Read` body in the C++
reference decoder (see `cpp/`). Field offsets are documented for
debugging; Python field names match the on-disk layout.
"""
from hdb_extract.classes.spec import (
    BaseInit, Byte, ByteAlt, ClassSpec, Dword, ObjList, Short, SubObj, Versioned,
)


def _icon_family(name: str, cid: int, size: int) -> ClassSpec:
    """Shared TLV grammar for 5 icon-family classes.

    Always : e/f/g/h/i/j as dwords.
    V > 3  : k as dword.
    """
    return ClassSpec(
        name=name, class_id=cid, size_bytes=size, parent="VCObject",
        ops=(
            BaseInit(),
            Dword(0x65, "e_at_0x28"),
            Dword(0x66, "f_at_0x2c"),
            Dword(0x67, "g_at_0x30"),
            Dword(0x68, "h_at_0x34"),
            Dword(0x69, "i_at_0x3c"),
            Dword(0x6a, "j_at_0x40"),
            Versioned(3, (Dword(0x6b, "k_at_0x38"),)),
        ),
        notes="Icon family — shared reader, 7 dwords.",
    )


def _email_family(name: str, cid: int) -> ClassSpec:
    """VCEmail / VCEmailRead / VCEmailPending — all have 1 polymorphic sub-obj."""
    return ClassSpec(
        name=name, class_id=cid, size_bytes=0x78, parent="VCObject",
        ops=(BaseInit(), SubObj("sub_f_at_0x3c")),
        notes="Email family — single polymorphic sub-object at +0x3c via ",
    )


def _list_family(name: str, cid: int, size: int = 0x44) -> ClassSpec:
    """A *List class — base + polymorphic obj_list (NPlt + NPTc)."""
    return ClassSpec(
        name=name, class_id=cid, size_bytes=size, parent="VCObject",
        ops=(BaseInit(), ObjList("items")),
        notes="List family — iterates polymorphic children via ",
    )


def _conv_history(name: str, cid: int) -> ClassSpec:
    """VCConversationHistory and VCPDANotes share the email-family pattern."""
    return ClassSpec(
        name=name, class_id=cid, size_bytes=0x80, parent="VCObject",
        ops=(BaseInit(), SubObj("sub_f_at_0x3c")),
        notes="Family pda methods — sub-object via ",
    )


ALL_CLASSES = (
    # --- Scene graph (0x27..0x2d) -------------------------------------
    ClassSpec(
        name="VCTitle", class_id=0x27, size_bytes=0x80, parent="VCObject",
        ops=(
            BaseInit(),
            Versioned(1, (ObjList("children"),)),
            SubObj("sub_f"),
            SubObj("sub_h"),
            SubObj("sub_g"),
            SubObj("sub_i"),
            Dword(0x6a, "j"),
            Dword(0x6b, "k"),
            Short(0x6c, "l"),
            Short(0x6d, "m"),
        ),
        notes=" v>1 adds children list. Read order : f,h,g,i (NOT alphabetical).",
    ),
    ClassSpec(
        name="VCNode", class_id=0x28, size_bytes=0x70, parent="VCObject",
        ops=(BaseInit(), ObjList("children"), Dword(0x66, "f"), Dword(0x67, "g")),
        notes=" children list + 2 dwords.",
    ),
    ClassSpec(
        name="VCLocaton", class_id=0x29, size_bytes=0x70, parent="VCObject",
        ops=(
            BaseInit(),
            ObjList("children"),
            Dword(0x66, "f"),
            Dword(0x67, "g"),
            Dword(0x68, "h"),
            Byte(0x69, "i"),
        ),
        notes=" (sic: 'VCLocaton' typo in the original)",
    ),
    ClassSpec(
        name="VCViewPoint", class_id=0x2a, size_bytes=0x60, parent="VCObject",
        ops=(
            BaseInit(),
            ObjList("items"),
            SubObj("sub_f"),
            Dword(0x67, "g"),
            Dword(0x68, "h"),
        ),
        notes="",
    ),
    ClassSpec(
        name="VCView", class_id=0x2b, size_bytes=0x60, parent="VCObject",
        ops=(
            BaseInit(),
            Dword(0x65, "e_at_0x28"),
            Dword(0x66, "f_at_0x2c"),
            Dword(0x67, "g_at_0x30"),
            Dword(0x68, "h_at_0x34"),
            Dword(0x69, "i_at_0x38"),
            Dword(0x6a, "j_at_0x3c"),
        ),
        notes=" 6 dwords sequential.",
    ),
    ClassSpec(
        name="VCCharacter", class_id=0x2c, size_bytes=0x30, parent="VCObject",
        ops=(BaseInit(), Dword(0x65, "e")),
        notes=" 1 dword.",
    ),
    ClassSpec(
        name="VCCharView", class_id=0x2d, size_bytes=0x60, parent="VCObject",
        ops=(
            BaseInit(),
            Dword(0x65, "e"),
            SubObj("sub_f"),
            Dword(0x67, "g"),
            Dword(0x68, "h"),
            Dword(0x69, "i"),
            Dword(0x6a, "j"),
        ),
        notes="",
    ),

    # --- VCName + Conversations + Nav (0x30..0x33) --------------------
    ClassSpec(
        name="VCName", class_id=0x30, size_bytes=0x40, parent="VCObject",
        ops=(BaseInit(), SubObj("sub_e"), SubObj("sub_f")),
        notes=" 2 polymorphic sub-objects.",
    ),
    ClassSpec(
        name="VCConversation", class_id=0x31, size_bytes=0x80, parent="VCObject",
        ops=(
            BaseInit(),
            Dword(0x65, "e_at_0x28"),
            Dword(0x66, "f_at_0x2c"),
            Dword(0x68, "h_at_0x34"),
            Dword(0x6a, "j_at_0x3c"),
            Versioned(2, (Dword(0x67, "g_at_0x30"), Dword(0x69, "i_at_0x38"))),
            Versioned(3, (ByteAlt(0x6b, "k_at_0x40"),)),
        ),
        notes=" v>2 adds g,i; v>3 adds k (byte_alt slot 0x7c).",
    ),
    _list_family("VCConversationList", 0x32),
    ClassSpec(
        name="VCNav", class_id=0x33, size_bytes=0x38, parent="VCObject",
        ops=(
            BaseInit(),
            Dword(0x65, "e_at_0x28"),
            Dword(0x66, "f_at_0x2c"),
            Dword(0x67, "g_at_0x30"),
        ),
        notes=" 3 sequential dwords.",
    ),
    _list_family("VCNav_List", 0x34),

    # --- Asset / Explorable / HotSpot (0x35..0x3a) --------------------
    ClassSpec(
        name="VCAssetRef", class_id=0x35, size_bytes=0x40, parent="VCObject",
        ops=(
            BaseInit(),
            SubObj("sub_e"),
            Dword(0x66, "f"),
            Dword(0x67, "g"),
            Byte(0x68, "h"),
            Byte(0x69, "i"),
        ),
        notes="",
    ),
    _list_family("VCAssetRefList", 0x36),
    ClassSpec(
        name="VCExplorable", class_id=0x37, size_bytes=0x40, parent="VCObject",
        ops=(BaseInit(), SubObj("sub_e"), Dword(0x66, "f")),
        notes="",
    ),
    _list_family("VCExplorableList", 0x38),
    ClassSpec(
        name="VCHotSpot", class_id=0x39, size_bytes=0x40, parent="VCObject",
        ops=(BaseInit(), SubObj("sub_e"), Dword(0x66, "f")),
        notes=" coords + action_ids carried in sub_e VCAssetRef sub-object.",
    ),
    _list_family("VCHotSpotList", 0x3a),

    # --- StdHotSpot / AssetCategory / Cursor / Default (0x3b..0x40) ---
    _list_family("VCStdHotSpotList", 0x3b),
    ClassSpec(
        name="VCAssetCategory", class_id=0x3c, size_bytes=0x30, parent="VCObject",
        ops=(BaseInit(), Dword(0x65, "e"), Byte(0x66, "f")),
        notes="",
    ),
    ClassSpec(
        name="VCCursor", class_id=0x3d, size_bytes=0x40, parent="VCObject",
        ops=(BaseInit(), SubObj("sub_e"), Dword(0x66, "f"), Dword(0x67, "g")),
        notes="",
    ),
    ClassSpec(
        name="VCDefaultCursors", class_id=0x3e, size_bytes=0x40, parent="VCObject",
        ops=(BaseInit(), ObjList("items")),
        notes=" Inline sub-collection.",
    ),
    ClassSpec(
        name="VCDefaultHotSpots", class_id=0x3f, size_bytes=0x40, parent="VCObject",
        ops=(BaseInit(), ObjList("items")),
        notes="",
    ),

    # --- Action / Blob / Player / Enabled (0x41..0x45) ----------------
    ClassSpec(
        name="VCAction", class_id=0x41, size_bytes=0x40, parent="VCObject",
        ops=(BaseInit(), SubObj("expr"), Byte(0x66, "action_type")),
        notes=" expr = polymorphic Expression sub-object.",
    ),
    _list_family("VCActionList", 0x42),
    ClassSpec(
        name="VCBlob", class_id=0x43, size_bytes=0x30, parent="VCObject",
        ops=(BaseInit(), SubObj("sub_e")),
        notes="base + 1 polymorphic sub-object at +0x28.",
    ),
    ClassSpec(
        name="VCPlayer", class_id=0x44, size_bytes=0x30, parent="VCObject",
        ops=(BaseInit(), Dword(0x65, "e")),
        notes=" (thunks to VCCharacter::Read).",
    ),
    ClassSpec(
        name="VCEnabled", class_id=0x45, size_bytes=0x30, parent="VCObject",
        ops=(BaseInit(), Dword(0x65, "e"), Byte(0x66, "f"), Byte(0x67, "g")),
        notes="",
    ),

    # --- Icons (0x46..0x4b) -------------------------------------------
    _icon_family("VCEmotionIcon", 0x46, 0x44),
    _icon_family("VCActionIcon", 0x47, 0x44),
    _icon_family("VCIdeaIcon", 0x48, 0x44),
    _icon_family("VCEvidenceIcon", 0x49, 0x44),
    # 0x4a is VCInventoryIcon synthetic? The reference decoder jumps from 0x49 to 0x4b.
    _icon_family("VCInventory", 0x4b, 0x44),

    # --- IdeaMap + Interface + String (0x4c..0x52) -------------------
    ClassSpec(
        name="VCIdeaMap", class_id=0x4c, size_bytes=0x40, parent="VCObject",
        ops=(BaseInit(), Dword(0x65, "e"), Dword(0x66, "f")),
        notes="",
    ),
    _list_family("VCIdeaMapList", 0x4d),
    ClassSpec(
        name="VCIFaceLayout", class_id=0x4e, size_bytes=0x40, parent="VCObject",
        ops=(BaseInit(), ObjList("items"), Dword(0x66, "f"), Byte(0x67, "g")),
        notes="",
    ),
    ClassSpec(
        name="VCInterfaceItem", class_id=0x4f, size_bytes=0x40, parent="VCObject",
        ops=(
            BaseInit(),
            SubObj("sub_e"),
            Dword(0x66, "f"),
            Dword(0x67, "g"),
            Short(0x68, "h"),
            Versioned(4, (Short(0x69, "i"),)),
        ),
        notes=" v>4 adds short i. Native uses slot 0xc0 for h ; the reference decoder uses read_short.",
    ),
    ClassSpec(
        name="VCString", class_id=0x50, size_bytes=0x30, parent="VCObject",
        ops=(BaseInit(), SubObj("sub_e")),
        notes=" 1 polymorphic sub-object.",
    ),
    ClassSpec(
        name="VCTrigger", class_id=0x51, size_bytes=0x40, parent="VCObject",
        ops=(
            BaseInit(),
            Dword(0x65, "e"),
            Byte(0x66, "f"),
        ),
        notes=" (annotated VCTrigger_Attach  but structurally a Read): "
              "calls CNeoPersist::readObject (slot12 ) + slot 0x9c read_dword('e') "
              "stored at this+0xE4 + slot 0x80 read_byte('f') stored at this+0xE8. "
              "Confirmed byte-direct against the on-disk record structure.",
    ),
    _list_family("VCTriggerList", 0x52),

    # --- Variable / StdAction (0x53..0x55) ---------------------------
    ClassSpec(
        name="VCVariable", class_id=0x53, size_bytes=0x40, parent="VCObject",
        ops=(
            BaseInit(),
            SubObj("sub_e"),
            Dword(0x67, "g"),
            Dword(0x69, "i"),
            Byte(0x68, "h"),
            Byte(0x66, "f"),
        ),
        notes=" NOTE order : e(sub),g,i,h,f.",
    ),
    ClassSpec(
        name="VCStdAction", class_id=0x54, size_bytes=0x40, parent="VCObject",
        ops=(BaseInit(), Dword(0x65, "e"), Dword(0x66, "f"), Dword(0x68, "h")),
        notes=" Skips tag 'g' (intentional).",
    ),
    _list_family("VCStdActionList", 0x55),

    # --- PDA + GameState (0x56..0x59) --------------------------------
    _conv_history("VCConversationHistory", 0x56),
    ClassSpec(
        name="VCGameState", class_id=0x57, size_bytes=0x1ac, parent="VCObject",
        ops=(
            BaseInit(),
            *(Dword(0x65 + i, f"flag_{chr(ord('e')+i)}") for i in range(9)),  # e..m
            Dword(0x6e, "flag_n"),
            Dword(0x6f, "flag_o"),
            Dword(0x70, "flag_p"),
            Dword(0x71, "flag_q"),
            Dword(0x72, "flag_r"),
            Dword(0x73, "flag_s"),
            SubObj("sub_t"),
            SubObj("sub_u"),
            SubObj("sub_v"),
            SubObj("sub_w"),
            SubObj("sub_x"),
            *(Byte(0x79 + i, f"byte_flag_{i}") for i in range(7)),
            Dword(0x80, "dword_0x80"),
            Byte(0x81, "byte_0x81"),
            Dword(0x82, "dword_0x82"),
            Dword(0x83, "counter_0"),
            Dword(0x84, "counter_1"),
            Short(0x85, "short_value"),
        ),
        notes=" Master singleton — 670 GAM-style game vars accessed via this.",
    ),
    _conv_history("VCPDANotes", 0x58),
    ClassSpec(
        name="VCPhoto", class_id=0x59, size_bytes=0x78, parent="VCObject",
        ops=(
            BaseInit(),
            SubObj("sub_e"),
            Dword(0x66, "f"),
            Dword(0x67, "g"),
            SubObj("sub_h"),
            Byte(0x69, "i"),
            Byte(0x6a, "j"),
        ),
        notes="",
    ),

    # --- Emails (0x5a..0x5c) -----------------------------------------
    _email_family("VCEmail", 0x5a),
    _email_family("VCEmailRead", 0x5b),
    _email_family("VCEmailPending", 0x5c),

    # --- Extra "list_family" classes that don't have a singleton VC*  ---
    # VCCharViewList is referenced but the reference decoder ports it as a generic list.
    _list_family("VCCharViewList", 0x2e),
)
