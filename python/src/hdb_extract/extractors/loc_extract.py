"""Localization DLL decoder.

The X-Files retail release ships four resource-only Windows PE DLLs:

  * ``XFilesc.dll`` — credits (cast & staff)
  * ``XFilese.dll`` — error messages and system requirements
  * ``XFiless.dll`` — save / scene titles
  * ``XFilest.dll`` — in-game text (the largest)

All four declare ``LANG_FRENCH_STANDARD (0x040C)`` and contain a single
resource type ``RT_STRING (6)``. Strings are stored UTF-16LE, grouped
into "string blocks" of 16 entries each (standard Windows resource
format).

This extractor reads any of them via ``pefile`` and returns a flat
``{id: text}`` mapping.

Reference: ``the format notes`` for the per-DLL category
breakdown.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any

try:
    import pefile  # type: ignore[import-not-found]
except ImportError as e:
    raise ImportError(
        "loc_extract requires `pefile`. Install via:\n"
        "    pip install pefile\n"
    ) from e


# Windows resource type ID for RT_STRING.
RT_STRING = 6

# Per-DLL category mapping (see the format notes).
DLL_CATEGORIES = {
    "XFilesc": "credits",
    "XFilese": "errors",
    "XFiless": "save_titles",
    "XFilest": "game_text",
}


class LocaleDLL:
    """Parsed view of one localization DLL.

    Attributes:
        path: source file path (if known).
        language: Win32 language ID (e.g. 0x040C = French / France).
        strings: dict mapping string ID (int) -> decoded UTF-16LE string.
        category: short name based on filename ("credits", "errors", ...).
    """

    def __init__(self, path: str | Path):
        self.path = Path(path)
        self.category = self._infer_category()
        pe = pefile.PE(str(self.path), fast_load=True)
        pe.parse_data_directories(directories=[
            pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]
        ])
        self.language: int | None = None
        self.strings: dict[int, str] = {}
        self._parse(pe)

    def _infer_category(self) -> str:
        stem = self.path.stem
        if stem in DLL_CATEGORIES:
            return DLL_CATEGORIES[stem]
        # Try suffix letter (XFilesX where X in {c,e,s,t}).
        if len(stem) == 7 and stem.startswith("XFiles"):
            suffix = stem[6].lower()
            for k, v in DLL_CATEGORIES.items():
                if k.endswith(suffix):
                    return v
        return "unknown"

    def _parse(self, pe: "pefile.PE") -> None:
        if not hasattr(pe, "DIRECTORY_ENTRY_RESOURCE"):
            return
        for type_entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
            if type_entry.id != RT_STRING:
                continue
            for block_entry in type_entry.directory.entries:
                # block_entry.id (Windows resource layout): the resource
                # ID for the block; the 16 strings inside have IDs
                # (block_id - 1) * 16 + 0..15.
                block_id = block_entry.id
                for lang_entry in block_entry.directory.entries:
                    if self.language is None:
                        self.language = lang_entry.id
                    rva = lang_entry.data.struct.OffsetToData
                    size = lang_entry.data.struct.Size
                    data = pe.get_memory_mapped_image()[rva:rva + size]
                    self._decode_string_block(block_id, data)

    def _decode_string_block(self, block_id: int, data: bytes) -> None:
        """Decode one 16-string block.

        Layout (Windows resource string block):
          for i in 0..15:
            uint16_t length_in_chars (little-endian)
            wchar_t  chars[length]   (no null terminator)

        A length of 0 means "no string at this slot".
        """
        offset = 0
        base_id = (block_id - 1) * 16
        for i in range(16):
            if offset + 2 > len(data):
                break
            length = int.from_bytes(data[offset:offset + 2], "little")
            offset += 2
            if length == 0:
                continue
            byte_len = length * 2
            if offset + byte_len > len(data):
                break
            raw = data[offset:offset + byte_len]
            offset += byte_len
            try:
                text = raw.decode("utf-16-le")
            except UnicodeDecodeError:
                text = raw.decode("utf-16-le", errors="replace")
            self.strings[base_id + i] = text

    def summary(self) -> dict[str, Any]:
        return {
            "file": str(self.path),
            "category": self.category,
            "language_id": self.language,
            "language_hex": (f"0x{self.language:04x}" if self.language is not None
                             else None),
            "string_count": len(self.strings),
            "id_range": (
                [min(self.strings), max(self.strings)] if self.strings else None
            ),
        }


def extract_locale_to_json(dll_path: str | Path,
                            out_path: str | Path) -> dict[str, Any]:
    """Extract one DLL's strings to JSON. Returns the payload dict."""
    loc = LocaleDLL(dll_path)
    payload: dict[str, Any] = loc.summary()
    payload["strings"] = {str(k): v for k, v in sorted(loc.strings.items())}
    out_p = Path(out_path)
    out_p.parent.mkdir(parents=True, exist_ok=True)
    out_p.write_text(json.dumps(payload, indent=2, ensure_ascii=False),
                     encoding="utf-8")
    return payload


def merge_locales(dll_paths: list[str | Path]) -> dict[str, Any]:
    """Merge several DLLs into a single locale dict keyed by category.

    Returns ``{category: {id: text}}``. Useful for shipping one unified
    JSON to a fan-port instead of four separate files.
    """
    out: dict[str, Any] = {"language_id": None, "categories": {}}
    for p in dll_paths:
        loc = LocaleDLL(p)
        if out["language_id"] is None:
            out["language_id"] = loc.language
        out["categories"][loc.category] = {
            str(k): v for k, v in sorted(loc.strings.items())
        }
    return out
