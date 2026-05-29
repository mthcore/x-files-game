"""PFF archive extractor — NeoLogic PICT archive format.

The X-Files game ships its non-HDB assets bundled in `.PFF` archive files
(`X.PFF`, `X7.PFF`, and per-scene archives under `XG/`, etc.). The PFF
format is a simple offset-table archive originally designed to bundle
Apple QuickDraw PICT images.

Wire format (little-endian throughout):

    struct PFF_Header {
        char     magic[4];              // "PFF " (0x50 0x46 0x46 0x20)
        uint32_t entry_count;
        uint32_t offsets[entry_count + 1];   // offsets[i] = start of entry i
                                              // offsets[count] = file size (sentinel)
    };
    // Zero padding from end-of-header to offsets[0] (aligned to 0x200).
    // Entry payloads concatenated at offsets[0..count-1].

Entry size for index `i` = `offsets[i+1] - offsets[i]`.

The byte-direct round-trip is validated: reading the file, reserializing
from the parsed structure, and comparing produces an identical byte
stream. See `OSS/docs/PFF_FORMAT.md`.

Port from `native/scripts/pff_walker.py` (v0.1 internal research script).
"""
from __future__ import annotations

import struct
from io import BytesIO
from pathlib import Path
from typing import Iterator


PFF_MAGIC = b"PFF "
PADDING_ALIGN = 0x200


class PffArchive:
    """Owning parser for a `.PFF` archive.

    The full file is read into memory at construction time (typical
    archives are 0.5–50 MB). Use :meth:`entry` to access an entry's raw
    bytes without copying, or :meth:`iter_entries` for sequential access.
    """

    def __init__(self, raw: bytes, *, path: Path | None = None):
        self.raw = raw
        self.path = path
        self._parse()

    @classmethod
    def from_path(cls, path: str | Path) -> "PffArchive":
        p = Path(path)
        return cls(p.read_bytes(), path=p)

    def _parse(self) -> None:
        if len(self.raw) < 8:
            raise ValueError(f"PFF too small ({len(self.raw)} bytes)")
        if self.raw[:4] != PFF_MAGIC:
            raise ValueError(f"Not a PFF archive: bad magic {self.raw[:4]!r}")
        self.count = struct.unpack_from("<I", self.raw, 4)[0]
        if self.count > (len(self.raw) - 8) // 4:
            raise ValueError(f"declared entry_count={self.count} exceeds file size")
        self.offsets = list(
            struct.unpack_from(f"<{self.count + 1}I", self.raw, 8)
        )
        # Bounds checks.
        if self.offsets[-1] != len(self.raw):
            raise ValueError(
                f"offsets[count]=0x{self.offsets[-1]:x} != file size "
                f"0x{len(self.raw):x}"
            )
        if self.offsets[0] < 8 + (self.count + 1) * 4:
            raise ValueError("first entry overlaps the offset table")
        # Padding zone must be all zeros.
        table_end = 8 + (self.count + 1) * 4
        if any(b != 0 for b in self.raw[table_end:self.offsets[0]]):
            raise ValueError("padding zone between offset table and first entry contains non-zero bytes")

    @property
    def entry_count(self) -> int:
        return self.count

    @property
    def header_size(self) -> int:
        return 8 + (self.count + 1) * 4

    @property
    def padding_size(self) -> int:
        return self.offsets[0] - self.header_size

    def entry(self, idx: int) -> bytes:
        """Return entry `idx` bytes (view; copy with `bytes(...)` if needed)."""
        if idx < 0 or idx >= self.count:
            raise IndexError(f"entry {idx} out of range (0..{self.count - 1})")
        return self.raw[self.offsets[idx] : self.offsets[idx + 1]]

    def entry_size(self, idx: int) -> int:
        if idx < 0 or idx >= self.count:
            raise IndexError(f"entry {idx} out of range")
        return self.offsets[idx + 1] - self.offsets[idx]

    def iter_entries(self) -> Iterator[tuple[int, bytes]]:
        for i in range(self.count):
            yield i, self.entry(i)

    def serialize(self) -> bytes:
        """Reconstruct the byte stream from parsed fields.

        Returns a byte-identical copy of the source file. Used by
        ``roundtrip_ok`` and by callers that want to write back the
        archive after manual edits.
        """
        buf = BytesIO()
        buf.write(PFF_MAGIC)
        buf.write(struct.pack("<I", self.count))
        for off in self.offsets:
            buf.write(struct.pack("<I", off))
        cur = buf.tell()
        if cur > self.offsets[0]:
            raise ValueError(
                f"offset table ({cur}B) > first entry offset (0x{self.offsets[0]:x})"
            )
        buf.write(b"\x00" * (self.offsets[0] - cur))
        for i in range(self.count):
            buf.write(self.raw[self.offsets[i] : self.offsets[i + 1]])
        return buf.getvalue()

    def roundtrip_ok(self) -> bool:
        return self.serialize() == self.raw


def extract_pff_to_dir(path: str | Path, out_dir: str | Path) -> int:
    """Extract every entry of a PFF archive to `out_dir`.

    File naming: ``entry_NNNN.bin`` where NNNN is the zero-padded index.
    Returns the number of entries extracted.
    """
    pff = PffArchive.from_path(path)
    dst = Path(out_dir)
    dst.mkdir(parents=True, exist_ok=True)
    for i in range(pff.count):
        (dst / f"entry_{i:04d}.bin").write_bytes(pff.entry(i))
    return pff.count
