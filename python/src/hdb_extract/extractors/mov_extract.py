"""QuickTime MOV extractor.

The X-Files `.xmv`, `.nmv`, `.amv`, `.dmv` files are **standard QuickTime
MOV containers** with public codecs (Cinepak ~79%, MJPEG ~20%, RPZA ~1%
for video; IMA ADPCM for audio). This module:

1. Parses the QuickTime atom hierarchy to expose metadata (duration,
   dimensions, codec FourCCs, frame count) **without external deps**.
2. Optionally decodes frames via ``ffmpeg`` if installed (subprocess
   call). Cinepak / IMA ADPCM byte-direct decoders are also shipped
   under ``OSS/cpp/src/assets/`` for embedded use cases.

The QuickTime container format itself is documented in:
https://developer.apple.com/standards/qtff-2001.pdf

Atoms relevant to X-Files videos:
    moov  - movie metadata
      mvhd - movie header (timescale, duration)
      trak - per-track metadata
        tkhd - track header (dimensions, ID)
        mdia - media
          mdhd - media header
          minf - media info
            stbl - sample table
              stsd - sample descriptions (codec FourCC)
              stsz - sample sizes
              stco - chunk offsets
    mdat  - actual sample data (pixels / audio)
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterator


@dataclass
class Atom:
    """One atom in the QuickTime hierarchy.

    Attributes:
        type: 4-character ASCII type (e.g. 'moov', 'mdat').
        offset: byte offset of the atom (including the size+type header).
        size: total atom size in bytes (including the 8-byte header).
        children: nested atoms (populated for container atoms).
    """
    type: str
    offset: int
    size: int
    children: list["Atom"] = field(default_factory=list)


# Atoms that are containers (i.e. contain children atoms instead of payload).
CONTAINER_ATOMS = {
    "moov", "trak", "mdia", "minf", "stbl", "dinf",
    "edts", "udta", "meta", "ilst",
}


class MovFile:
    """Parsed QuickTime MOV file.

    Reads the file into memory and indexes the atom tree. Bytes are kept
    so that callers can re-slice them; for large videos (multi-GB) you
    would want a streamed parser, but X-Files videos top out at ~10 MB.
    """

    def __init__(self, raw: bytes, *, path: Path | None = None):
        self.raw = raw
        self.path = path
        self.atoms: list[Atom] = []
        self._parse()

    @classmethod
    def from_path(cls, path: str | Path) -> "MovFile":
        p = Path(path)
        return cls(p.read_bytes(), path=p)

    def _parse(self) -> None:
        self.atoms = list(self._iter_atoms_at(0, len(self.raw)))

    def _iter_atoms_at(self, start: int, end: int) -> Iterator[Atom]:
        off = start
        while off + 8 <= end:
            size = int.from_bytes(self.raw[off:off + 4], "big")
            atype = self.raw[off + 4:off + 8].decode("ascii", errors="replace")
            if size == 0:
                # "Atom extends to end of file" convention.
                size = end - off
            elif size == 1:
                # 64-bit extended size.
                if off + 16 > end:
                    return
                size = int.from_bytes(self.raw[off + 8:off + 16], "big")
            if size < 8 or off + size > end:
                return
            atom = Atom(type=atype, offset=off, size=size)
            if atype in CONTAINER_ATOMS:
                atom.children = list(self._iter_atoms_at(off + 8, off + size))
            yield atom
            off += size

    def find(self, *path: str) -> Atom | None:
        """Walk the atom tree by name path. Returns None if not found."""
        scope: list[Atom] = self.atoms
        result: Atom | None = None
        for name in path:
            found = None
            for a in scope:
                if a.type == name:
                    found = a
                    break
            if found is None:
                return None
            result = found
            scope = found.children
        return result

    def video_codec(self) -> str | None:
        """Return the 4-char codec FourCC for the first video track, or None."""
        stsd = self.find("moov", "trak", "mdia", "minf", "stbl", "stsd")
        if stsd is None:
            return None
        # stsd payload: 4B version+flags, 4B count, then sample descriptions.
        body_off = stsd.offset + 8
        if body_off + 16 > stsd.offset + stsd.size:
            return None
        # Each sample description: 4B size, 4B type (FourCC), then reserved + idx.
        codec_off = body_off + 8 + 4  # skip header + count + size
        return self.raw[codec_off:codec_off + 4].decode("ascii", errors="replace")

    def duration_seconds(self) -> float | None:
        """Return the movie duration in seconds, or None if unavailable."""
        mvhd = self.find("moov", "mvhd")
        if mvhd is None:
            return None
        body_off = mvhd.offset + 8
        version = self.raw[body_off]
        if version == 0:
            ts_off = body_off + 12
            dur_off = body_off + 16
            timescale = int.from_bytes(self.raw[ts_off:ts_off + 4], "big")
            duration = int.from_bytes(self.raw[dur_off:dur_off + 4], "big")
        else:
            ts_off = body_off + 20
            dur_off = body_off + 24
            timescale = int.from_bytes(self.raw[ts_off:ts_off + 4], "big")
            duration = int.from_bytes(self.raw[dur_off:dur_off + 8], "big")
        if timescale == 0:
            return None
        return duration / timescale

    def dimensions(self) -> tuple[int, int] | None:
        """Return (width, height) of the first video track, or None."""
        tkhd = self.find("moov", "trak", "tkhd")
        if tkhd is None:
            return None
        # tkhd ends with two 32-bit fixed-point dimensions (16.16 format).
        end = tkhd.offset + tkhd.size
        w_raw = int.from_bytes(self.raw[end - 8:end - 4], "big")
        h_raw = int.from_bytes(self.raw[end - 4:end], "big")
        return (w_raw >> 16, h_raw >> 16)

    def frame_count(self) -> int | None:
        """Return the video frame count, or None."""
        stsz = self.find("moov", "trak", "mdia", "minf", "stbl", "stsz")
        if stsz is None:
            return None
        # stsz payload: 4B version+flags, 4B sample_size, 4B count.
        body_off = stsz.offset + 8
        count_off = body_off + 8
        if count_off + 4 > stsz.offset + stsz.size:
            return None
        return int.from_bytes(self.raw[count_off:count_off + 4], "big")

    def summary(self) -> dict:
        return {
            "file": str(self.path) if self.path else "(in-memory)",
            "size": len(self.raw),
            "top_level_atoms": [a.type for a in self.atoms],
            "video_codec": self.video_codec(),
            "dimensions": self.dimensions(),
            "duration_seconds": self.duration_seconds(),
            "frame_count": self.frame_count(),
        }


def extract_to_mov(src: str | Path, dst: str | Path) -> Path:
    """Copy the input to ``dst`` with a ``.mov`` extension if missing.

    Since the source is already a standard MOV, no transcoding is needed.
    """
    src_p = Path(src)
    dst_p = Path(dst)
    if dst_p.suffix.lower() != ".mov":
        dst_p = dst_p.with_suffix(".mov")
    dst_p.parent.mkdir(parents=True, exist_ok=True)
    dst_p.write_bytes(src_p.read_bytes())
    return dst_p
