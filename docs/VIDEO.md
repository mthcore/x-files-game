# Video — QuickTime MOV files (.xmv / .nmv / .amv / .dmv)

The X-Files game ships ~2 300 video files, totalling ~3 GB. Despite
their varied extensions, they are **all standard QuickTime MOV
containers** — the same format Apple defined in 1991 and that
`.mov` files use today. Decoders for the contained codecs are public
domain or already shipped with this project.

## File extensions

| Ext    | Role                                  | Common codec | Examples |
|--------|---------------------------------------|--------------|----------|
| `.xmv` | Scene videos (Mulder/Scully cinematic)| Cinepak      | `XV/17923.XMV` (8.5 MB, 18 s, 538 frames @ 600×240) |
| `.nmv` | Navigation transitions                | MJPEG        | `NAV1.NMV` (6 MB, 180 s, 166 frames @ 600×240) |
| `.amv` | Audio-only (ambient)                  | IMA ADPCM    | `XS/26274.AMV` |
| `.dmv` | Audio-only (dialog)                   | IMA ADPCM    | `XS/20962.DMV` |

The extension is purely a hint to the game loader — the **container is
identical** in all four cases.

## Codec distribution (X-Files, retail)

From the production map of 2 432 scene productions (extracted via
`python -m hdb_extract extract`):

| Codec   | FourCC  | Files | % of total |
|---------|---------|------:|-----------:|
| Cinepak | `cvid`  | 1 331 |     79 %   |
| MJPEG   | `jpeg`  |   331 |     20 %   |
| RPZA    | `rpza`  |    12 |      1 %   |

Audio tracks (when present) use **IMA ADPCM 4-bit** at 22 050 Hz mono,
the de-facto Apple standard of the era.

## On-disk layout

The QuickTime MOV format is well-documented (see the Apple
[QuickTime File Format spec](https://developer.apple.com/standards/qtff-2001.pdf)).
Atoms relevant to X-Files videos:

```
moov            container       movie metadata
  mvhd          leaf            movie header (timescale, duration)
  trak          container       per-track metadata
    tkhd        leaf            track header (dimensions)
    mdia        container       media
      minf      container       media info
        stbl    container       sample table
          stsd  leaf            sample descriptions (codec FourCC)
          stsz  leaf            sample sizes
          stco  leaf            chunk offsets
mdat            leaf            actual frame payloads
```

The `free` atom (rarely seen) is padding inserted by the original
encoder and is preserved byte-direct by our parser.

## Python API

```python
from hdb_extract.extractors.mov_extract import MovFile

m = MovFile.from_path("XV/17923.XMV")
print(m.video_codec())         # 'cvid'
print(m.dimensions())          # (600, 240)
print(m.duration_seconds())    # 17.93
print(m.frame_count())         # 538

# Walk the atom tree manually
mdat = m.find("mdat")
print(mdat.offset, mdat.size)  # location + length of the frame data

# Quick summary (JSON-ready)
print(m.summary())
```

The parser is **pure Python**, no external runtime deps.

## CLI

```bash
# Inspect
python -m hdb_extract mov XV/17923.XMV
# file       : XV/17923.XMV
# size       : 8,554,329 bytes
# atoms      : moov free mdat
# codec      : cvid
# dimensions : (600, 240)
# duration   : 17.93s
# frames     : 538

# Copy with a .mov extension for tools that need it
python -m hdb_extract mov XV/17923.XMV --out=out/17923.mov

# Then any standard player can read it:
ffplay out/17923.mov
ffprobe -hide_banner out/17923.mov
```

## Decoding frames

Since the container is standard MOV with public codecs, **any modern
tool can decode the frames** without code from this project:

```bash
# Extract every frame as PNG
ffmpeg -i out/17923.mov out/frame_%04d.png

# Or transcode to H.264 for sharing
ffmpeg -i out/17923.mov -c:v libx264 -crf 18 out/17923.mp4
```

If you cannot rely on ``ffmpeg`` (e.g. an embedded fan-port), this
project ships **byte-direct C++ decoders** for the two most common
codecs:

| Codec   | Header                                | Tested against            |
|---------|---------------------------------------|---------------------------|
| Cinepak | `cpp/include/assets/cinepak_decoder.h` | FFmpeg, frame-by-frame    |
| IMA ADPCM | `cpp/include/assets/ima_adpcm.h`    | FFmpeg, sample-by-sample  |
| QT container | `cpp/include/assets/qt_container.h` | every X-Files video       |

MJPEG and RPZA are not implemented in this project (use FFmpeg or
ImageMagick — they are public codecs with mature open-source decoders).

## C++ API (byte-direct decoder)

```cpp
#include "assets/qt_container.h"
#include "assets/cinepak_decoder.h"

xfiles::assets::QTContainer qt;
qt.parse(file_bytes.data(), file_bytes.size());
const auto codec = qt.video_codec();           // e.g. "cvid"
const auto [w, h] = qt.dimensions();
const auto frame_offsets = qt.video_frame_offsets();

if (codec == "cvid") {
    xfiles::assets::CinepakDecoder dec;
    for (const auto& [off, size] : frame_offsets) {
        std::vector<uint8_t> rgb(w * h * 3);
        dec.decode_frame(file_bytes.data() + off, size, w, h, rgb.data());
        // do something with the RGB frame
    }
}
```

See `OSS/cpp/include/assets/` for the full public headers and
`OSS/cpp/src/assets/` for the implementations.

## Performance

On a 2024 laptop (Apple M3 / Linux x86), the pure-Python parser handles
~50 MB/s for atom enumeration. For frame decoding, defer to the C++
decoders (orders of magnitude faster) or to FFmpeg.

## Why ship the MOV parser if QuickTime is standard?

Two reasons:

1. **No dependency**: the Python parser is pure stdlib. A fan-port
   reading thousands of XMV files at boot doesn't want to shell out
   to ffmpeg on every file just to read the duration.
2. **X-Files-specific edge cases**: the QuickTime spec allows many
   optional features (edit lists, multiple tracks, B-frames). The
   X-Files videos use a deliberately narrow subset, and this parser is
   tested against all 2 300 files in the retail release. Edge cases
   that don't appear in X-Files are not implemented.

## Status

- ✅ Atom hierarchy parser (pure Python, no deps).
- ✅ Cinepak decoder (C++ byte-direct, tested against FFmpeg).
- ✅ IMA ADPCM decoder (C++ byte-direct, tested against FFmpeg).
- ✅ Tested on the 2 300+ X-Files MOV files.
- 🟡 MJPEG / RPZA decoding: delegate to FFmpeg or ImageMagick.
- 🟡 Python-side frame decoding: planned for v0.3 (Pillow + ctypes binding).
