# PFF — NeoLogic PICT archive

The `.PFF` file format is a thin archive container used by HyperBole to
bundle large numbers of small assets (originally Apple QuickDraw PICT
images, hence the name). It is **byte-direct round-trippable** through
the project's parser.

## When is PFF used?

The X-Files game ships with two top-level PFF archives:

| File     | Size       | Entries | Role                             |
|----------|-----------:|--------:|----------------------------------|
| `X.PFF`  | 43 672 120 | 727     | Main asset bundle (PICT images)  |
| `X7.PFF` | 654 084    | 12      | Auxiliary / alternate asset set  |

A handful of additional `.PFF` files live inside `XG/` (game graphics
directory) for per-scene asset packaging.

## On-disk format

Little-endian throughout (despite the HDB being big-endian — PFF is a
plain Windows/x86 format).

```
+0   char     magic[4]              = "PFF " (0x50 0x46 0x46 0x20)
+4   uint32_t entry_count
+8   uint32_t offsets[entry_count + 1]   // see below
+8+4*(count+1)  zero padding to align offsets[0] to 0x200
+offsets[0]     entry 0 payload
+offsets[1]     entry 1 payload
...
+offsets[count-1] entry count-1 payload
+offsets[count]   = end of file (sentinel)
```

The offset table has `entry_count + 1` entries. Index `i` (for
`0 ≤ i < entry_count`) gives the file offset where entry `i` starts;
index `entry_count` is a **sentinel equal to the total file size**.
Entry sizes are computed as `offsets[i+1] - offsets[i]`.

After the offset table, the file is **zero-padded** so that the first
entry payload starts at a 0x200-byte (= 512-byte sector) boundary. This
mirrors the original Mac OS HFS allocation strategy.

## Example: X.PFF

```
file       : X.PFF
size       : 43,672,120 bytes
entries    : 727
header     : 2920 bytes        (8 + 728 × 4)
padding    : 2908 bytes        (zero-fill to reach 0x16c4 = sector 11)
first entry @ 0x000016c4
roundtrip byte-identical : True
```

## Example: X7.PFF

```
file       : X7.PFF
size       : 654,084 bytes
entries    : 12
header     : 60 bytes          (8 + 13 × 4)
padding    : 48 bytes          (zero-fill to reach 0x6C = sector 0)
first entry @ 0x0000006c
roundtrip byte-identical : True
```

## Entry payloads

Each entry is a raw PICT (Apple QuickDraw) image **without the standard
512-byte PICT header**. To convert an extracted entry into a standalone
PICT file readable by tools like Preview.app or convert tools, prepend
512 bytes of zero:

```python
with open("entry_0000.bin", "rb") as f, open("entry_0000.pict", "wb") as out:
    out.write(b"\x00" * 512)
    out.write(f.read())
```

PICT decoding itself is out of scope for v0.2 of this project; pipe the
output to ImageMagick (`convert -define pict:headerless=true ...`) or to
the `pict_decoder.cpp` shipped under `OSS/cpp/src/assets/`.

## CLI usage

```bash
# Inspect (read-only)
python -m hdb_extract pff /path/to/X.PFF

# Extract every entry to a directory
python -m hdb_extract pff /path/to/X.PFF --out=out/pff_entries/
# Produces entry_0000.bin, entry_0001.bin, ...
```

## Python API

```python
from hdb_extract.extractors.pff_extract import PffArchive

archive = PffArchive.from_path("/path/to/X.PFF")
print(archive.entry_count)             # 727
print(archive.entry_size(0))           # bytes of entry 0
print(archive.entry(0)[:8].hex())      # first 8 bytes of entry 0
assert archive.roundtrip_ok()          # serialize back == source
```

## Why is this trivial?

The PFF format pre-dates modern container formats (Zip, Tar). It is
essentially a list of `(offset, length)` pairs followed by payloads. No
compression, no checksums, no encryption, no indexing.

The format analysis took **under one hour**: the PFF walker is about
152 lines including the round-trip validation. The byte-direct round-trip
is what gives us confidence the parser is correct.

## Round-trip guarantee

```python
from hdb_extract.extractors.pff_extract import PffArchive
archive = PffArchive.from_path("X.PFF")
assert archive.roundtrip_ok()  # True on all known X-Files PFF files
```

This guarantee means: if you extract every entry and then re-pack them
in the same order with the same padding, you get the exact same file
back. No information is lost.

## Compared to HDB

| Aspect              | HDB (NeoPersist 3.0) | PFF (NeoLogic PICT)  |
|---------------------|----------------------|----------------------|
| Endianness          | Big-endian           | Little-endian        |
| Structured records  | Yes (51 VC* classes) | No (opaque blobs)    |
| Indexing            | B-tree               | Flat offset table    |
| Versioning          | Per-object           | None                 |
| Round-trip          | ✓ byte-identical     | ✓ byte-identical     |
| Analysis cost       | ~140h                | <1h                  |

The two formats are **independent** and require **separate decoders**.
This project handles both.
