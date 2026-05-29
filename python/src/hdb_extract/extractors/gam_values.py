"""Decode GAM variable values byte-direct.

Each state-variable record stores, relative to its CNeoPersist marker magic
(`00 00 00 01`) that immediately follows the variable name:

    magic+4..+5  flags
    magic+6..+7  storage mark (u16)
    magic+8      VALUE  (u8 — bool 0/1 or small int)
    magic+9      class/type tag (u8)

The value offset is confirmed two ways: (1) across multiple real savegames the
byte at magic+8 of `bAIHauling_GetShovel` flips 0<->1 in lockstep with picking
up the shovel, and `Inventory_iPhotoSmolnikoff` likewise; (2) read over a whole
save, the `b…` (boolean) variables come out 0/1 in ~96% of cases and `i…`
(integer) variables come out small — exactly the expected shape.

Wider integers may use the bytes after magic+8; this decoder reports the single
value byte (sufficient for the boolean / small-int majority) plus the raw
following bytes so nothing is hidden.
"""
from __future__ import annotations

import re

MAGIC = b"\x00\x00\x00\x01"
_IDENT = re.compile(rb"([A-Za-z_][A-Za-z0-9_]{2,46})\x00")
_SCOPES = ("IFL_", "Inventory_", "NODE_", "ANODE_", "SHANKSN", "PDANOTES_",
           "Pref_", "EMAIL_", "EndGame_", "NAVTAG_", "bAI")


def _is_var(n: str) -> bool:
    return (n.startswith(_SCOPES)
            or (n[:1] in "biscn" and len(n) > 2 and n[1:2].isupper()))


def _vtype(n: str) -> str:
    base = n.split("_")[-1]
    return {"b": "bool", "i": "int", "c": "counter", "n": "id",
            "s": "string"}.get(base[:1], "other")


_DICT_RE = re.compile(rb"\x01\x2b\x27[\x80-\xff]\x20?([A-Za-z_][A-Za-z0-9_]{1,46})\x00")
_RUN_RE = re.compile(rb"[\x20-\x7e]{4,120}")
_DOC_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]{2,46}) \((.+)\)$")
_LEGEND_RE = re.compile(r"([A-Za-z0-9])\s*=\s*([^,]+)")


def decode_gam_constants(raw: bytes) -> dict[str, int]:
    """The `01 2b 27 8x` dictionary family: name -> integer constant value.

    Value is byte-direct and provable (IFL_cPhone0..9 == '0'..'9', font/phone
    codes == their mnemonic letter). Returns {name: value}.
    """
    import struct
    out: dict[str, int] = {}
    for m in _DICT_RE.finditer(raw):
        s = m.start()
        if s < 12:
            continue
        out.setdefault(m.group(1).decode("latin1"),
                       struct.unpack_from(">I", raw, s - 6)[0])
    return out


def decode_gam_docs(raw: bytes, limit: int = 220_000) -> dict:
    """Authoring annotations embedded as literal strings (byte-direct).

    Returns {documented_variables, enum_legends, condition_labels}.
    """
    raw = raw[:limit]
    docs: dict[str, str] = {}
    legends: dict[str, dict] = {}
    questions: set[str] = set()
    for m in _RUN_RE.finditer(raw):
        t = m.group().decode("latin1").strip()
        d = _DOC_RE.match(t)
        if d:
            name, desc = d.group(1), d.group(2).strip()
            docs.setdefault(name, desc)
            leg = dict(_LEGEND_RE.findall(desc))
            if len(leg) >= 2:
                legends.setdefault(name, {k: v.strip() for k, v in leg.items()})
        elif (t.endswith("?") and 8 <= len(t) <= 60 and " " in t
              and sum(c.isalpha() or c == " " for c in t) / len(t) > 0.85):
            questions.add(t)
    return {"documented_variables": docs, "enum_legends": legends,
            "condition_labels": sorted(questions)}


def decode_gam_values(raw: bytes, limit: int = 200_000) -> dict[str, dict]:
    """Return {name: {value, type, class_tag, raw}} read byte-direct from a GAM."""
    raw = raw[:limit]
    out: dict[str, dict] = {}
    for m in _IDENT.finditer(raw):
        n = m.group(1).decode("latin1")
        if not _is_var(n) or n in out:
            continue
        mg = raw.find(MAGIC, m.end(), m.end() + 12)
        if mg < 0 or mg + 12 > len(raw):
            continue
        out[n] = {
            "value": raw[mg + 8],
            "type": _vtype(n),
            "class_tag": raw[mg + 9],
            "raw": raw[mg + 6:mg + 12].hex(" "),  # mark + value + class + next2
            "certainty": "byte-direct",
        }
    return out
