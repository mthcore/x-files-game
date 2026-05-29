"""Decode VC* records from HDB using PackedHDBSerializer (G1 cracked).

For each CNeoIDList ID pointer, follow its offset_or_size, then try
EACH candidate VC* class and keep the **best match** :

  - Maximum bytes_consumed (= most structured class fits)
  - Tie-break by preferring classes with more concrete fields decoded
    (not just sub-objects that came back unsupported)

This avoids the first-match bias that would tag everything as VCTitle.
"""
from __future__ import annotations

from typing import Any

from hdb_extract.classes.registry import CLASSES_BY_ID, apply_read, spec_for
from hdb_extract.container.container import HdbContainer
from hdb_extract.format.neoidlist import parse_neoidlist_record
from hdb_extract.serializer.packed import PackedHDBSerializer


def _serialize(v):
    """JSON-safe : bytes→hex, recurse into dicts/lists."""
    if isinstance(v, bytes):
        return v.hex()
    if isinstance(v, dict):
        return {k: _serialize(x) for k, x in v.items()}
    if isinstance(v, list):
        return [_serialize(x) for x in v]
    return v


def _score_decode(result: dict, cursor: int) -> int:
    """Score how 'good' a decode is. Higher = more confidence.

    Strongly penalize unsupported sub-objects (their class_ids are
    out-of-registry, indicating mis-alignment). Reward concrete primitives
    with plausible values.
    """
    score = cursor  # 1 point per byte consumed
    if not isinstance(result, dict):
        return score
    for k, v in result.items():
        if k.startswith("__"):
            continue
        if isinstance(v, dict):
            if v.get("__unsupported__"):
                # Each garbage class_id costs more than 100 bytes of progress.
                # This stops VCGameState from winning by reading random bytes.
                score -= 500
            elif v.get("_g1_pending"):
                score -= 50
            else:
                score += 20
        elif isinstance(v, list):
            # Empty or huge lists are suspicious.
            if 0 < len(v) < 1000:
                score += 15
            else:
                score -= 10
        elif isinstance(v, int):
            if 0 <= v < 100_000:
                score += 5
            elif v == 0:
                score += 2
            else:
                # Very large integers are usually garbage.
                score -= 5
        elif isinstance(v, str):
            # Real strings have printable chars and reasonable length.
            if v and all(32 <= ord(c) < 127 or c in "\t\n" for c in v[:64]):
                score += 10
            else:
                score -= 5
        elif isinstance(v, bytes):
            score += 3
    return score


def _confidence(result: dict) -> str:
    """Confidence level based on unsupported sub-objects ratio."""
    n_unsup = 0
    n_total = 0
    if isinstance(result, dict):
        for k, v in result.items():
            if k.startswith("__"):
                continue
            if isinstance(v, dict) and "__class_id__" in v:
                n_total += 1
                if v.get("__unsupported__"):
                    n_unsup += 1
    if n_total == 0:
        return "high"
    ratio = n_unsup / n_total
    if ratio == 0:
        return "high"
    if ratio < 0.5:
        return "medium"
    return "low"


def parse_record_marker(raw: bytes, offset: int) -> dict | None:
    """Parse the 8-byte HDB record marker (from leaf_walker.cpp).

    Wire format @ offset :
      [tag(1B)][sub_tag(1B)][00 00 00 01 magic][flag16 BE]

    Returns dict with marker fields, or None if no valid marker at offset.
    The payload follows immediately at offset+8.
    """
    if offset + 8 > len(raw):
        return None
    b = raw[offset:offset + 8]
    if not (0xC0 <= b[0] <= 0xCF):
        return None
    if b[2] != 0x00 or b[3] != 0x00 or b[4] != 0x00 or b[5] != 0x01:
        return None
    return {
        "marker_tag": b[0],
        "marker_sub_tag": b[1],
        "marker_flag16": (b[6] << 8) | b[7],
    }


def try_decode_at(
    raw: bytes,
    offset: int,
    class_id: int,
    max_bytes: int = 512,
) -> tuple[dict[str, Any] | None, int]:
    """Decode `raw[offset:]` as `class_id`. Returns (record_dict, score) or (None, 0)."""
    spec = spec_for(class_id)
    if spec is None:
        return None, 0
    buf = raw[offset:offset + max_bytes]
    ser = PackedHDBSerializer(buf, absolute_offset=offset)
    try:
        result = apply_read(ser, spec)
    except Exception:
        return None, 0
    if ser.error or ser.cursor == 0:
        return None, 0
    score = _score_decode(result, ser.cursor)
    return {
        "class_id": class_id,
        "class_name": spec.name,
        "offset": offset,
        "bytes_consumed": ser.cursor,
        "score": score,
        "confidence": _confidence(result),
        "fields": {k: _serialize(v) for k, v in result.items()
                   if not k.startswith("__")},
    }, score


def best_decode_at(
    raw: bytes,
    offset: int,
    skip_marker: bool = True,
) -> dict[str, Any] | None:
    """Try every known class, return the highest-scoring decode.

    If `skip_marker` is True (default), checks for an 8-byte HDB record
    marker at `offset` and skips past it before decoding. The marker is
    returned in the result under `__marker__`.
    """
    marker = None
    decode_offset = offset
    if skip_marker:
        marker = parse_record_marker(raw, offset)
        if marker is not None:
            decode_offset = offset + 8

    best = None
    best_score = -1
    for cid in CLASSES_BY_ID:
        d, score = try_decode_at(raw, decode_offset, cid)
        if d is None:
            continue
        if score > best_score:
            best_score = score
            best = d
    if best is not None and marker is not None:
        best["__marker__"] = marker
        best["marker_offset"] = offset
        best["payload_offset"] = decode_offset
    return best


def extract_decoded_records(container: HdbContainer) -> dict[str, Any]:
    """Decode every CNeoIDList target as the best-matching VC* record."""
    raw = container.raw
    decoded: list[dict] = []
    by_class: dict[str, int] = {}
    no_match = 0

    for page_id, page in container.iter_pages():
        if not page:
            continue
        page_abs = container.page_offset(page_id)
        idx = 0
        while True:
            idx = page.find(b"ID  ", idx)
            if idx < 0:
                break
            if idx >= 16 and idx + 14 <= len(page):
                rec_off_in_page = idx - 16
                try:
                    rec = parse_neoidlist_record(page, rec_off_in_page)
                except Exception:
                    idx += 4
                    continue
                if rec.is_valid:
                    target = rec.offset_or_size
                    if 0 < target < len(raw):
                        match = best_decode_at(raw, target)
                        if match:
                            decoded.append({
                                "source_neoidlist_offset": page_abs + rec_off_in_page,
                                "target_offset": target,
                                **match,
                            })
                            by_class[match["class_name"]] = (
                                by_class.get(match["class_name"], 0) + 1
                            )
                        else:
                            no_match += 1
            idx += 4

    return {
        "_source": "PackedHDBSerializer applied to CNeoIDList target offsets "
                   "(bit-reversal + BE byte-swap, "
                   "8-byte record marker skip)",
        "_methodology": "1) Parse 8-byte record marker [tag][sub_tag]"
                        "[00 00 00 01][flag16 BE], 2) decode payload from "
                        "marker_offset+8, 3) best-match scoring by_class.",
        "_marker_format": "established from the on-disk record structure",
        "total_decoded": len(decoded),
        "total_no_match": no_match,
        "by_class": dict(sorted(by_class.items(), key=lambda x: -x[1])),
        "decoded_records": decoded,
    }
