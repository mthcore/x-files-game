"""Python port of the CNeoFileStream byte-direct on-disk encoding.

Format details (NeoPersist 3.0 wire format) :
- CNeoFileStream stores values big-endian (NeoAccess canonical layout).
- Helpers : readChunk, readLong, bswap32, reallyReadChunk, etc.

All reads are big-endian (NeoAccess canonical format from Mac origin).
The runtime swaps to host endianness on x86 (LE).

Notes on tags : the file format DOES NOT contain inline TLV tags for fields
in the X-Files version (the `v4 > 4` version check is FALSE here, since
the format-version short at offset 20 of CNeoFormat is 0). Tags like 'null',
'ID  ' that appear in HDB are class-level OBJECT tags, not field tags.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass


@dataclass
class CNeoStream:
    """Byte-direct reader for NeoAccess HDB format.

    Mirrors CNeoFileStream behavior, established from the on-disk format :
    - All numbers are big-endian on disk
    - bswap32 applied when reading
    - 4096-byte IOBlock caching (we just use the full bytes)
    """
    data: bytes
    mark: int = 0    # = fMark in CNeoFileStream

    # CNeoStream::fInputFormat.version (from CNeoFormat+20)
    # In X-Files this is 0, so the v>4 conditional is always FALSE
    version: int = 0

    def __len__(self) -> int:
        return len(self.data)

    def setMark(self, pos: int) -> None:
        """Equivalent to CNeoStream::setMark(NeoMark)."""
        self.mark = pos

    def getMark(self) -> int:
        return self.mark

    # ---- Primitives byte-direct ----

    def readChunk(self, length: int, tag: int = 0) -> bytes:
        """ / - raw N-byte read. Tag is ignored at this level."""
        buf = self.data[self.mark:self.mark + length]
        if len(buf) != length:
            raise EOFError(f"readChunk: requested {length} bytes at mark={self.mark}, "
                           f"got {len(buf)}")
        self.mark += length
        return buf

    def readByte(self, tag: int = 0) -> int:
        """ - 1 byte unsigned."""
        return self.readChunk(1, tag)[0]

    def readChar(self, tag: int = 0) -> int:
        """ - 1 byte signed."""
        b = self.readChunk(1, tag)[0]
        return b - 256 if b >= 128 else b

    def readBoolean(self, tag: int = 0) -> bool:
        """ - 1 byte → bool (non-zero = true)."""
        return self.readByte(tag) != 0

    def readShort(self, tag: int = 0) -> int:
        """ - 2B BE → u16."""
        return struct.unpack(">H", self.readChunk(2, tag))[0]

    def readShortSigned(self, tag: int = 0) -> int:
        """Signed variant of readShort."""
        return struct.unpack(">h", self.readChunk(2, tag))[0]

    def readLong(self, tag: int = 0) -> int:
        """ (slot +0x9c) - 4B BE → u32."""
        return struct.unpack(">I", self.readChunk(4, tag))[0]

    def readLongSigned(self, tag: int = 0) -> int:
        """Signed variant of readLong."""
        return struct.unpack(">i", self.readChunk(4, tag))[0]

    def readLongTagged(self, tag: int) -> int:
        """ (slot +0x138) - 4B BE [+ 4B if version > 4].

        In X-Files, version is 0 so this is equivalent to readLong.
        """
        value = struct.unpack(">I", self.readChunk(4, tag))[0]
        if self.version > 4:
            self.readChunk(4, tag)   # discard tag
        return value

    def readBits(self, length: int, tag: int = 0) -> bytes:
        """ - same as readChunk; raw N bytes."""
        return self.readChunk(length, tag)

    def readNativeString(self, max_len: int = 255, tag: int = 0) -> str:
        """Pascal string : 1B length + chars (Mac classic native string).

        Per CNeoStream.cp readNativeString : on Mac the buffer is a CNeoString
        (Pascal string). On non-Mac, the format is still 1B + chars.
        """
        n = self.readByte(tag)
        if n > max_len:
            n = max_len
        raw = self.readChunk(n, tag)
        return raw.decode('latin-1', errors='replace')

    def readCString(self, max_len: int, tag: int = 0) -> str:
        """C-string : up to max_len bytes, null-terminated.

        Per CNeoStream.cp readString : reads aLength bytes total, assumes
        null termination within.
        """
        raw = self.readChunk(max_len, tag)
        nul = raw.find(b'\x00')
        if nul >= 0:
            raw = raw[:nul]
        return raw.decode('latin-1', errors='replace')

    def readDouble(self, tag: int = 0) -> float:
        """8B IEEE 754 BE."""
        return struct.unpack(">d", self.readChunk(8, tag))[0]

    def readFloat(self, tag: int = 0) -> float:
        """4B IEEE 754 BE."""
        return struct.unpack(">f", self.readChunk(4, tag))[0]

    def readBlob(self, mark: int, length: int, tag: int = 0) -> bytes:
        """Equivalent to CNeoStream::readBlob - seek + readChunk + restore mark."""
        old_mark = self.mark
        self.setMark(mark)
        try:
            return self.readChunk(length, tag)
        finally:
            self.setMark(old_mark)

    # ---- Object/Persistent reads ----

    def readCNeoPersistHeader(self) -> dict:
        """Read CNeoPersist::readObject layout : 2B packed flags + 4B version.

        Per CNeoPersist.h : kNeoPersistFileLength = 2 + sizeof(NeoVersion) = 6 bytes
        when qNeoVersions is defined.
        """
        flags_packed = self.readShort(tag=0x3E50666C)  # 'NPfl' (>Pfl)
        version = self.readLong(tag=0x3E507672)        # 'NPvr' (>Pvr)

        # Unpack the bit fields per CNeoPersist.h
        return {
            "fLeaf":      bool(flags_packed & 0x0001),
            "fRoot":      bool(flags_packed & 0x0002),
            "fTemporary": bool(flags_packed & 0x0004),
            "fLocal":     (flags_packed >> 3) & 0x0003,
            "fShareFill": bool(flags_packed & 0x0020),
            "fBusy":      bool(flags_packed & 0x0040),
            "fBool1":     bool(flags_packed & 0x0080),
            "fBool2":     bool(flags_packed & 0x0100),
            "fBool3":     bool(flags_packed & 0x0200),
            # fReserved : 0x0C00
            "fPeerDirty": (flags_packed >> 12) & 0x0003,
            "fDirty":     (flags_packed >> 14) & 0x0003,
            "fVersion":   version,
        }

    def readRecordMarker(self) -> dict:
        """Read the on-disk record marker = CNeoPersist header + class_id.

        Validated byte-direct on 5 records of different classes : the 8-byte
        marker is EXACTLY :
          [NPfl 2B][NPvr 4B BE][class_id 2B BE]
        = kNeoPersistFileLength (6) + class_id (2) = 8 bytes total.

        After this, the class-specific payload begins (read by the class's
        own readObject method).
        """
        packed_flags = self.readShort()
        version = self.readLong()
        class_id = self.readShort()

        return {
            "packed_flags": packed_flags,
            "packed_flags_hex": f"0x{packed_flags:04x}",
            "version": version,
            "class_id": class_id,
            "class_id_hex": f"0x{class_id:04x}",
            # Decoded flag bits per CNeoPersist.h
            "fLeaf":      bool(packed_flags & 0x0001),
            "fRoot":      bool(packed_flags & 0x0002),
            "fTemporary": bool(packed_flags & 0x0004),
            "fLocal":     (packed_flags >> 3) & 0x0003,
            "fShareFill": bool(packed_flags & 0x0020),
            "fBusy":      bool(packed_flags & 0x0040),
            "fBool1":     bool(packed_flags & 0x0080),
            "fBool2":     bool(packed_flags & 0x0100),
            "fBool3":     bool(packed_flags & 0x0200),
            "fPeerDirty": (packed_flags >> 12) & 0x0003,
            "fDirty":     (packed_flags >> 14) & 0x0003,
        }


# ==== TLV tags from NeoAccess (constants) ====

# NeoPersist field tags (>P*) — used as runtime hints, NOT stored in X-Files file
TAG_NPfl = 0x3E50666C   # '>Pfl' - flags
TAG_NPlf = 0x3E506C66   # '>Plf' - leaf
TAG_NPrt = 0x3E507274   # '>Prt' - root
TAG_NPtm = 0x3E50746D   # '>Ptm' - temporary
TAG_NPlo = 0x3E506C6F   # '>Plo' - local
TAG_NPb1 = 0x3E506231   # '>Pb1' - bool1
TAG_NPb2 = 0x3E506232   # '>Pb2' - bool2
TAG_NPb3 = 0x3E506233   # '>Pb3' - bool3
TAG_NPbz = 0x3E50627A   # '>Pbz' - busy
TAG_NPdy = 0x3E506479   # '>Pdy' - dirty
TAG_NPrc = 0x3E507263   # '>Prc' - refcnt
TAG_NPpr = 0x3E507072   # '>Ppr' - parent
TAG_NPvr = 0x3E507672   # '>Pvr' - version

# Generic tags (from NeoTypes.h)
TAG_NULL = 0x6E756C6C   # 'null' - kNeoNoTag
TAG_OBJ  = 0x7F626A20   # 'obj ' (with 0x7F prefix byte) - kNeoObjectTag
TAG_ALL  = 0x616C6C20   # 'all ' - kNeoAllTag
TAG_NCSP = 0x6E637370   # 'ncsp' - kNeoCanSupport
TAG_NGKY = 0x6E676B79   # 'ngky' - kNeoGetKey

# NeoPersist-Type tags observed in X-Files VCObject::Read
TAG_NPTc = 0x4E505463   # 'NPTc' - NeoPersist Type code
TAG_NPTl = 0x4E50546C   # 'NPTl' - NeoPersist Type long
TAG_NPTo = 0x4E50546F   # 'NPTo' - NeoPersist Type object
TAG_NPlt = 0x4E506C74   # 'NPlt' - NeoPersist list
TAG_NPrn = 0x4E50726E   # 'NPrn' - NeoPersist root nodeID
TAG_NPdu = 0x4E506475   # 'NPdu' - NeoPersist dirty/used
TAG_ID   = 0x49442020   # 'ID  ' - object ID

# NeoAccess internal class IDs (from NeoTypes.h)
CLASS_kNeoNullClass     = 0x00
CLASS_kNeoPersist       = 0x01
CLASS_kNeoBlob          = 0x02
CLASS_kNeoClass         = 0x03
CLASS_kNeoSubclass      = 0x04
CLASS_kNeoFreeList      = 0x05
CLASS_kNeoInode         = 0x06
CLASS_kNeoIDIndex       = 0x07
CLASS_kNeoParentIndex   = 0x08
CLASS_kNeoGenericIndex  = 0x09
CLASS_kNeoPartMgr       = 0x0A
CLASS_kNeoIDList        = 0x0B
