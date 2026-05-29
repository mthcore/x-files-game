"""Master HDB ID resolver — mirrors the C++ reference decoder (see `cpp/`).

Critical insight from the on-disk structure :

  - Each leaf page (tag 0xC3) contains 8-byte typed records.
  - Each record has a `value32` field (the persistent ID).
  - Each record has a `flag16` field (the **TARGET PAGE INDEX** where the
    payload TLV lives).
  - Resolution = O(1) via a flat hash : id -> (page_idx, rec_idx).
  - Once you have the record, the payload is in `pages[record.flag16]`
    (256 bytes, TLV-decoded).

This is how the B-tree is shortcutted : scan every leaf once at boot to
build a flat hash, then lookups are O(1).

Extension layer (opt-in, see ``MasterHDBResolver.extend_with_neoidlist_entries``)

  The default resolver scans only page-aligned 8-byte markers in 0xCX
  pages, which covers ~28k IDs. The HDB also contains thousands of
  ``[offset BE u32][id BE u32]`` pair tables embedded inside data pages
  and inside the payloads of CNeoIDList (0x0B) / CNeoIDIndex (0x07)
  records. Walking these tables exposes IDs that have no page-aligned
  8-byte record (their payload is reached only by indirection).

  The extension is opt-in to preserve the original behaviour and existing
  test invariants. Once enabled it merges new IDs into ``id_index`` with
  byte-direct file offsets (``id -> file_offset`` rather than
  ``(page_idx, rec_idx)`` — both shapes are stored in
  ``extended_offsets`` so callers can request a raw offset).

Class-scoped extension (see ``extend_with_class_index``)

  Beyond CNeoIDList (0x0B) / CNeoIDIndex (0x07), the recon
  ``pair_table_classes.json`` shows that many other CNeoPersist classes
  embed ``[marker_offset u32, id u32]`` pair tables in their record
  payloads (for example part_0x10, 0x12, VCActionIcon, VCPhoto, ...).
  ``extend_with_class_index(class_ids)`` walks the payloads of those
  record types specifically, looking for the same pair pattern. It is a
  pure superset of the existing ``neoidlist_records`` scope (which is
  literally ``extend_with_class_index({0x07, 0x0B})``) and a strict subset
  of ``global`` (first-wins collision policy: an ID found globally is
  never re-bound by a later class-scoped scan).
"""
from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Iterable, Literal, Optional

from hdb_extract.container.container import HdbContainer


PAGE_SIZE = 256
HEADER_SIZE = 32
MARKER_MAGIC = bytes([0x00, 0x00, 0x00, 0x01])

# Upper bound on a valid NeoID. The recon (see
# G:/TheXFiles/order_recovery/scripts/master_index_extended.py) showed that
# real conversation/scene IDs stay well below this ceiling; values above are
# sentinels or unrelated 32-bit data that happened to follow a marker offset.
NEOID_CEILING = 0x100000


@dataclass
class Record8:
    """An 8-byte typed record from a leaf page."""

    offset_in_page: int   # byte offset within the 256-byte page
    marker_tag: int       # bytes[0] : 0xC0..0xCF
    marker_sub_tag: int   # bytes[1]
    flag16: int           # bytes[6..7] BE : target page index for payload
    value32: int = 0      # bytes[+8..+11] BE if available : the ID
    next_offset: int = 0  # offset of next record in page (or PAGE_SIZE)


@dataclass
class RecordLocation:
    """Where a record is stored after id-resolution."""

    page_idx: int
    rec_idx: int
    record: Record8


def parse_page_records(page: bytes) -> list[Record8]:
    """Parse all 8-byte marker records in a page (256B).

    Each record starts on 8-byte alignment with the marker pattern.
    Returns records in page order.
    """
    recs: list[Record8] = []
    marker_offs: list[int] = []
    for off in range(0, PAGE_SIZE - 8 + 1, 8):
        if 0xC0 <= page[off] <= 0xCF and page[off + 2:off + 6] == MARKER_MAGIC:
            marker_offs.append(off)

    for i, off in enumerate(marker_offs):
        next_off = marker_offs[i + 1] if i + 1 < len(marker_offs) else PAGE_SIZE
        flag16 = (page[off + 6] << 8) | page[off + 7]
        value32 = 0
        if off + 12 <= PAGE_SIZE:
            value32 = struct.unpack_from(">I", page, off + 8)[0]
        recs.append(Record8(
            offset_in_page=off,
            marker_tag=page[off],
            marker_sub_tag=page[off + 1],
            flag16=flag16,
            value32=value32,
            next_offset=next_off,
        ))
    return recs


def classify_handle(
    value: int,
    hdb_size: int,
    *,
    raw: Optional[bytes] = None,
    id_ceiling: int = NEOID_CEILING,
) -> tuple[Literal["neoid", "direct-offset", "unknown"],
           Optional[int]]:
    """Classify a raw u32 conversation handle without a resolver attached.

    This is the resolver-less companion to
    :py:meth:`MasterHDBResolver.classify_handle`. It applies the same
    byte-direct rules but never returns ``"neoid"`` (NeoID resolution
    needs the resolver's index).

    - ``("direct-offset", value)`` when ``raw`` is provided, ``0 <= value
      < hdb_size``, and ``raw[value:value+8]`` is a real CNeoPersist
      marker (``0xC0..0xCF`` + ``00 00 00 01``).
    - ``("unknown", None)`` otherwise, including the value-is-zero case.

    The ``id_ceiling`` parameter is accepted for signature compatibility
    with the bound method and is not used here — values within the NeoID
    range cannot be resolved without an index.
    """
    if value == 0:
        return ("unknown", None)
    if 0 < value < hdb_size and raw is not None:
        if value + 8 <= len(raw):
            if 0xC0 <= raw[value] <= 0xCF and raw[value + 2:value + 6] == MARKER_MAGIC:
                return ("direct-offset", value)
    return ("unknown", None)


class MasterHDBResolver:
    """Flat-hash ID resolver — equivalent of MasterHDB::resolve_record.

    On attach, scans every leaf page once and builds id -> (page, rec_idx).
    """

    def __init__(self, container: HdbContainer):
        self.container = container
        self.id_index: dict[int, tuple[int, int]] = {}
        # ``extended_offsets`` is populated by extend_with_neoidlist_entries.
        # It maps id -> absolute file offset of the target marker, for IDs
        # discovered through embedded [offset, id] pair tables that do not
        # have a page-aligned 8-byte record.
        self.extended_offsets: dict[int, int] = {}
        self.btree_root: int | None = None
        self._build_index()

    def _build_index(self) -> None:
        page_count = len(self.container.raw) // PAGE_SIZE
        for pi in range(page_count):
            page = self.container.page(pi)
            if len(page) < PAGE_SIZE:
                continue
            # Remember first 0xC2 internal page as B-tree root candidate
            if self.btree_root is None and page[0] == 0xC2:
                self.btree_root = pi
            recs = parse_page_records(page)
            for ri, r in enumerate(recs):
                if r.value32 == 0:
                    continue
                # First-leaf-wins semantics (matches B-tree behaviour)
                if r.value32 not in self.id_index:
                    self.id_index[r.value32] = (pi, ri)

    def resolve_record(self, id_value: int) -> RecordLocation | None:
        """Return RecordLocation for `id_value`, or None if not found.

        IDs added via :py:meth:`extend_with_neoidlist_entries` do not have
        an 8-byte page-aligned record; for those, callers should use
        :py:meth:`resolve_offset` to obtain the absolute marker offset.
        This method returns ``None`` for extension-only IDs.
        """
        loc = self.id_index.get(id_value)
        if loc is None:
            return None
        page_idx, rec_idx = loc
        if page_idx < 0:
            # extension-only sentinel — no page-aligned record exists
            return None
        page = self.container.page(page_idx)
        recs = parse_page_records(page)
        if rec_idx >= len(recs):
            return None
        return RecordLocation(
            page_idx=page_idx,
            rec_idx=rec_idx,
            record=recs[rec_idx],
        )

    def resolve_payload(self, id_value: int) -> bytes | None:
        """Return the payload-page bytes for `id_value`, or None.

        The payload page is given by record.flag16 (target page index).
        Returns the full 256-byte page — callers should TLV-walk it.
        """
        loc = self.resolve_record(id_value)
        if loc is None:
            return None
        target_page = loc.record.flag16
        page_count = len(self.container.raw) // PAGE_SIZE
        if target_page >= page_count:
            return None
        return self.container.page(target_page)

    # ------------------------------------------------------------------
    # Extension layer — opt-in, preserves default behaviour.
    # ------------------------------------------------------------------

    def _scan_all_markers(self) -> dict[int, int]:
        """Return {absolute_file_offset: class_id} for every CNeoPersist
        marker found anywhere in the HDB (byte-direct, any alignment).

        The default :py:meth:`_build_index` only inspects 8-byte slots of
        0xCX pages. Many real records live in non-0xCX data pages and in
        B-tree internal pages; this scan finds every marker the file
        contains. Cost is one linear sweep.
        """
        raw = self.container.raw
        n = len(raw)
        markers: dict[int, int] = {}
        for off in range(HEADER_SIZE, n - 8 + 1):
            if not (0xC0 <= raw[off] <= 0xCF):
                continue
            if raw[off + 2:off + 6] != MARKER_MAGIC:
                continue
            markers[off] = (raw[off + 6] << 8) | raw[off + 7]
        return markers

    def extend_with_neoidlist_entries(
        self,
        *,
        scope: str = "global",
        id_ceiling: int = NEOID_CEILING,
    ) -> dict:
        """Extend ``id_index`` by walking ``[offset, id]`` pair tables.

        This mirrors the way the original engine reaches IDs that are
        registered in CNeoIDList / CNeoIDIndex records (and in the B-tree
        internal pages) rather than as page-aligned 8-byte records.

        Parameters
        ----------
        scope:
            ``"global"`` (default) scans the entire HDB at 4-byte alignment
            for ``[offset BE u32][id BE u32]`` pairs whose ``offset`` is
            the absolute file offset of a CNeoPersist marker. This is the
            scope validated by the recon (see
            ``order_recovery/scripts/master_index_extended.py``); it adds
            ~51k new IDs and resolves the conversation NeoIDs that the
            page-aligned index misses.

            ``"neoidlist_records"`` restricts the pair scan to the payloads
            of CNeoIDList (0x0B) and CNeoIDIndex (0x07) records only. This
            is the literal "scan the ID list records" reading; coverage is
            small (~few dozen IDs) because those records mostly hold short
            tag streams, not pair tables — kept available as a safer,
            narrower mode for callers that want the minimal extension.

        id_ceiling:
            Maximum NeoID accepted; values above are treated as unrelated
            32-bit data and ignored. Default is :data:`NEOID_CEILING`
            (0x100000), the empirical ceiling for valid IDs in this HDB.

        Semantics
        ---------
        - **Backward compatible**: IDs already in :attr:`id_index` keep
          their existing ``(page_idx, rec_idx)`` mapping. Newly found IDs
          get the sentinel mapping ``(-1, -1)`` and their absolute file
          offset is stored in :attr:`extended_offsets`.
        - **Collision policy**: first-wins, matching the default B-tree
          first-leaf semantics. If the same NeoID appears in multiple
          pair tables, the lowest file offset wins.
        - **Honesty**: every emitted ID corresponds to a literal byte
          pattern in the HDB; nothing is inferred.

        Returns
        -------
        dict with keys ``{added, before, after, markers, scope}``.
        """
        if scope not in {"global", "neoidlist_records"}:
            raise ValueError(
                f"scope must be 'global' or 'neoidlist_records', got {scope!r}"
            )
        raw = self.container.raw
        n = len(raw)
        markers = self._scan_all_markers()
        before = len(self.id_index)

        regions: list[tuple[int, int]]
        if scope == "global":
            regions = [(HEADER_SIZE, n)]
        else:
            # Scan only inside CNeoIDList (0x0B) and CNeoIDIndex (0x07)
            # record payloads. Payload = bytes between this marker and
            # the next marker in the same page.
            regions = []
            page_count = (n - HEADER_SIZE) // PAGE_SIZE
            for pi in range(page_count):
                page_off = HEADER_SIZE + pi * PAGE_SIZE
                if not (0xC0 <= raw[page_off] <= 0xCF):
                    continue
                slot_offs = [
                    o for o in range(0, PAGE_SIZE - 8 + 1, 8)
                    if (0xC0 <= raw[page_off + o] <= 0xCF
                        and raw[page_off + o + 2:page_off + o + 6]
                        == MARKER_MAGIC)
                ]
                for i, o in enumerate(slot_offs):
                    a = page_off + o
                    cid = (raw[a + 6] << 8) | raw[a + 7]
                    if cid not in (0x07, 0x0B):
                        continue
                    pay_start = a + 8
                    pay_end = (page_off + slot_offs[i + 1]
                               if i + 1 < len(slot_offs)
                               else page_off + PAGE_SIZE)
                    if pay_end > pay_start + 4:
                        regions.append((pay_start, pay_end))

        added = 0
        for start, end in regions:
            # Align to 4 bytes from start.
            base = start - (start & 3)
            if base < start:
                base += 4
            for pos in range(base, end - 7, 4):
                fo = struct.unpack_from(">I", raw, pos)[0]
                if fo not in markers:
                    continue
                rid = struct.unpack_from(">I", raw, pos + 4)[0]
                if rid == 0 or rid > id_ceiling:
                    continue
                if rid in self.id_index or rid in self.extended_offsets:
                    continue  # first-wins
                # Sentinel page/rec — caller should use extended_offsets
                # to get the absolute byte offset.
                self.id_index[rid] = (-1, -1)
                self.extended_offsets[rid] = fo
                added += 1

        return {
            "scope": scope,
            "before": before,
            "after": len(self.id_index),
            "added": added,
            "markers": len(markers),
        }

    def extend_with_class_index(
        self,
        class_ids: Iterable[int],
        *,
        id_ceiling: int = NEOID_CEILING,
    ) -> dict:
        """Extend ``id_index`` by walking ``[marker_offset, id]`` pair tables
        embedded in the payloads of every record whose class_id is in
        ``class_ids``.

        Each record's payload is the byte range from the marker end to the
        next byte-direct CNeoPersist marker in the file. The scan inspects
        4-byte-aligned positions; a pair is accepted only when the first
        u32 is the absolute file offset of a real marker AND the second
        u32 is a non-zero NeoID ``<= id_ceiling``.

        Parameters
        ----------
        class_ids:
            Iterable of CNeoPersist class ids to scan. Empty iterables are
            tolerated (the method becomes a no-op).
        id_ceiling:
            Maximum NeoID accepted; values above are treated as unrelated
            32-bit data and ignored. Default is :data:`NEOID_CEILING`.

        Semantics
        ---------
        - **First-wins**: an id already in :attr:`id_index` (page-aligned
          or previously extended) is never re-bound. This matches the
          B-tree first-leaf policy.
        - **Bounded by markers**: only ids paired with a real marker
          offset are accepted; nothing is inferred.
        - **Honesty**: returns an explicit report with per-class hit
          counts so callers can log exactly what happened.

        Returns
        -------
        dict ``{classes_scanned, ids_added, total_ids_before, total_ids_after,
        per_class}`` where ``per_class`` maps each class_id to a small dict
        with ``{records_scanned, pair_candidates, ids_added}``.
        """
        class_id_set = {int(c) for c in class_ids}
        raw = self.container.raw
        n = len(raw)
        before = len(self.id_index)
        markers = self._scan_all_markers()
        markers_offset_set = set(markers.keys())

        # Build record-payload regions for every requested class_id. A
        # payload runs from (marker_offset + 8) up to the next byte-direct
        # marker in the file (or EOF for the last record).
        markers_sorted = sorted(markers.items())
        regions_by_class: dict[int, list[tuple[int, int]]] = {
            c: [] for c in class_id_set
        }
        for i, (off, cid) in enumerate(markers_sorted):
            if cid not in class_id_set:
                continue
            pay_start = off + 8
            pay_end = (markers_sorted[i + 1][0]
                       if i + 1 < len(markers_sorted) else n)
            if pay_end > pay_start + 4:
                regions_by_class[cid].append((pay_start, pay_end))

        per_class: dict[int, dict] = {}
        total_added = 0
        for cid in class_id_set:
            regs = regions_by_class.get(cid, [])
            class_added = 0
            pair_candidates = 0
            for start, end in regs:
                base = start - (start & 3)
                if base < start:
                    base += 4
                for pos in range(base, end - 7, 4):
                    fo = struct.unpack_from(">I", raw, pos)[0]
                    if fo not in markers_offset_set:
                        continue
                    rid = struct.unpack_from(">I", raw, pos + 4)[0]
                    if rid == 0 or rid > id_ceiling:
                        continue
                    pair_candidates += 1
                    if rid in self.id_index or rid in self.extended_offsets:
                        continue  # first-wins
                    self.id_index[rid] = (-1, -1)
                    self.extended_offsets[rid] = fo
                    class_added += 1
            per_class[cid] = {
                "records_scanned": len(regs),
                "pair_candidates": pair_candidates,
                "ids_added": class_added,
            }
            total_added += class_added

        return {
            "classes_scanned": sorted(class_id_set),
            "ids_added": total_added,
            "total_ids_before": before,
            "total_ids_after": len(self.id_index),
            "per_class": per_class,
        }

    def resolve_offset(self, id_value: int) -> int | None:
        """Return the absolute file offset of the marker for ``id_value``,
        or ``None`` if unknown.

        Works for both the original page-aligned index and the
        extension-layer IDs registered via
        :py:meth:`extend_with_neoidlist_entries`.
        """
        if id_value in self.extended_offsets:
            return self.extended_offsets[id_value]
        loc = self.id_index.get(id_value)
        if loc is None or loc == (-1, -1):
            return None
        page_idx, rec_idx = loc
        page = self.container.page(page_idx)
        recs = parse_page_records(page)
        if rec_idx >= len(recs):
            return None
        return HEADER_SIZE + page_idx * PAGE_SIZE + recs[rec_idx].offset_in_page

    def classify_handle(
        self,
        value: int,
        hdb_size: int,
        *,
        id_ceiling: int = NEOID_CEILING,
    ) -> tuple[Literal["neoid", "direct-offset", "unknown"],
               Optional[int]]:
        """Classify a raw u32 conversation handle without inventing semantics.

        The returned tuple is one of:

        - ``("neoid", resolved_offset)`` — ``value`` is a known NeoID and
          resolves to a byte-direct CNeoPersist marker at
          ``resolved_offset``.
        - ``("direct-offset", value)`` — ``value`` is not a NeoID but lies
          inside the HDB file and points byte-direct at a real CNeoPersist
          marker (``0xC0..0xCF`` + ``00 00 00 01``). Only emitted when the
          bytes at that offset actually form a marker; never inferred.
        - ``("unknown", None)`` — neither path applies. The caller should
          report the field as ``undetermined``.

        The check is byte-direct; nothing is filled in by guess. Use the
        free-function :func:`classify_handle` when no resolver is at hand.
        """
        if value == 0:
            return ("unknown", None)
        # An ID known to the resolver is always returned as ``neoid``,
        # whether it came from the page-aligned scan (raw u32 from a
        # record) or the extended pair-table scan (capped at id_ceiling).
        if value in self.id_index or value in self.extended_offsets:
            off = self.resolve_offset(value)
            if off is not None:
                return ("neoid", off)
        return classify_handle(value, hdb_size, raw=self.container.raw,
                               id_ceiling=id_ceiling)

    def index_size(self) -> int:
        return len(self.id_index)

    def stats(self) -> dict:
        """Stats about the indexed pages."""
        page_count = len(self.container.raw) // PAGE_SIZE
        leaf_count = sum(1 for pi in range(page_count)
                         if self.container.page(pi)[:1] == b"\xc3")
        internal_count = sum(1 for pi in range(page_count)
                             if self.container.page(pi)[:1] == b"\xc2")
        return {
            "total_pages": page_count,
            "leaf_pages_0xC3": leaf_count,
            "internal_pages_0xC2": internal_count,
            "indexed_ids": len(self.id_index),
            "btree_root_page": self.btree_root,
        }
