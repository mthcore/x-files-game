"""hdb-extract CLI : inspect / audit / extract / verify / dump-class."""
from __future__ import annotations

import json
import sys
from pathlib import Path

import click

from hdb_extract.btree.walker import iter_leaf_pages, walk_records
from hdb_extract.classes.registry import (
    CLASSES_BY_ID,
    CLASSES_BY_NAME,
    apply_read,
    spec_for,
)
from hdb_extract.container.container import HdbContainer
from hdb_extract.container.pages import PAGE_TAG_NAMES
from hdb_extract.format.fourcc import fourcc_name
from hdb_extract.polymorphe.dispatcher import read_obj
from hdb_extract.serializer.cursor import HDBSerializer
from hdb_extract.serializer.tracking import ByteUsageMap, UsageCode


@click.group()
@click.version_option()
def main():
    """Extract XFILES.HDB to JSON + SQLite at 100% byte-direct fidelity."""


@main.command()
@click.argument("hdb_path", type=click.Path(exists=True, path_type=Path))
def inspect(hdb_path: Path):
    """Show header + page distribution for an HDB file."""
    c = HdbContainer.from_path(hdb_path)
    click.echo(f"file: {hdb_path}")
    click.echo(f"size: {len(c.raw):,} bytes  pages: {c.page_count:,}  "
               f"trailer: {c.trailer.size}B "
               f"({'zero' if c.trailer.is_zero_padding else 'non-zero'})")
    click.echo()
    click.echo("Header (big-endian, 32 bytes):")
    for f in ("version", "root_or_size", "record_count", "class_count",
              "unk_94982", "unk_1281", "max_or_limit", "page_size"):
        v = getattr(c.header, f)
        click.echo(f"  {f:14s} = {v:>12,d}  (0x{v:08x})")
    click.echo()
    click.echo("Page distribution (first byte tag) :")
    dist = c.page_type_distribution()
    structural = {0x00, 0xC0, 0xC2, 0xC3, 0xD2}
    others = 0
    shown = 0
    for tag, count in dist.most_common():
        if tag in structural or shown < 10:
            name = PAGE_TAG_NAMES.get(tag, "(data)")
            pct = count * 100 / sum(dist.values())
            click.echo(f"  0x{tag:02x}  {name:22s} {count:>7,d}  {pct:5.1f}%")
            shown += 1
        else:
            others += count
    if others:
        click.echo(f"  ...  (other data first-bytes)     {others:>7,d}")
    click.echo()
    click.echo(f"Classes documented (Python ClassSpec) : {len(CLASSES_BY_ID)}")
    rt = c.roundtrip_ok()
    click.echo(f"Container roundtrip byte-identical : {rt}")


@main.command()
@click.argument("hdb_path", type=click.Path(exists=True, path_type=Path))
@click.option("--out", "out_path", type=click.Path(path_type=Path),
              default="out/audit/coverage.md")
def audit(hdb_path: Path, out_path: Path):
    """Audit byte coverage of the HDB."""
    from hdb_extract.container.header import HEADER_SIZE
    from hdb_extract.container.pages import PAGE_SIZE

    c = HdbContainer.from_path(hdb_path)
    usage = ByteUsageMap(len(c.raw))

    # Mark header
    usage.mark(0, HEADER_SIZE, UsageCode.HEADER, kind="header")

    # Mark pages by tag
    for page_id, page in c.iter_pages():
        abs_off = c.page_offset(page_id)
        usage.mark(abs_off, 1, UsageCode.PAGE_TAG, page=page_id, tag=page[0])
        tag = page[0]
        if tag == 0x00:
            usage.mark(abs_off + 1, PAGE_SIZE - 1, UsageCode.PAGE_EMPTY)
        elif tag == 0xC0:
            usage.mark(abs_off + 1, PAGE_SIZE - 1, UsageCode.PAGE_FREED)
        elif tag in (0xC2, 0xD2):
            usage.mark(abs_off + 1, PAGE_SIZE - 1, UsageCode.PAGE_INTERNAL)
        elif tag == 0xC3:
            usage.mark(abs_off + 1, 5, UsageCode.PAGE_HEADER, kind="leaf_header5")
            usage.mark(abs_off + 6, PAGE_SIZE - 6, UsageCode.PAGE_LEAF_BODY,
                       kind="leaf_body_records")
        else:
            usage.mark(abs_off + 1, PAGE_SIZE - 1, UsageCode.PAGE_DATA,
                       kind="data_records", tag=tag)

    # Mark trailer
    trailer_off = HEADER_SIZE + c.page_count * PAGE_SIZE
    if c.trailer.is_zero_padding:
        usage.mark(trailer_off, c.trailer.size, UsageCode.TRAILER_ZERO,
                   kind="zero_padding")
    else:
        usage.mark(trailer_off, c.trailer.size, UsageCode.TRAILER_OTHER)

    # Walk leaves to identify records (best-effort).
    n_records = 0
    n_with_class = 0
    for rec in walk_records(c):
        n_records += 1
        if rec.class_id is not None:
            n_with_class += 1
            usage.mark(rec.abs_off, len(rec.raw), UsageCode.NEOIDLIST_HEADER,
                       class_id=rec.class_id)

    cov = usage.coverage()
    cons = usage.consumed_count()
    pct = usage.consumed_pct()

    lines = [
        "# HDB Coverage Audit — hdb_extract MVP",
        "",
        f"Source : `{hdb_path}` ({len(c.raw):,} bytes)",
        f"Pages : {c.page_count:,} of {PAGE_SIZE} bytes each",
        f"Trailer : {c.trailer.size} bytes ({'zero pad' if c.trailer.is_zero_padding else 'non-zero'})",
        f"Records walked : {n_records:,}  (class_id identified : {n_with_class:,})",
        "",
        "## Coverage by usage code",
        "",
        "| Code | Bytes | % |",
        "| ---- | ----- | --- |",
    ]
    for code, count in sorted(cov.items(), key=lambda x: -x[1]):
        lines.append(f"| {code} | {count:,} | {count * 100 / len(c.raw):.2f}% |")
    lines += [
        "",
        f"**Byte-direct consumed** : {cons:,} ({pct:.2f}%)",
        f"**Unknown** : {len(c.raw) - cons:,} ({100 - pct:.2f}%)",
    ]

    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines), encoding="utf-8")
    click.echo(f"wrote {out_path}")
    click.echo(f"byte-direct = {cons:,} / {len(c.raw):,} = {pct:.2f}%")


@main.command()
@click.argument("hdb_path", type=click.Path(exists=True, path_type=Path))
@click.option("--out", "out_dir", type=click.Path(path_type=Path),
              default="out")
def extract(hdb_path: Path, out_dir: Path):
    """Extract HDB records to JSON files."""
    from hdb_extract.extractors import (
        dump_classes,
        dump_neoidlist,
        dump_page_index,
        dump_production_paths,
        dump_strings,
    )

    c = HdbContainer.from_path(hdb_path)
    out_dir = Path(out_dir)
    json_dir = out_dir / "json"
    json_dir.mkdir(parents=True, exist_ok=True)

    # 1. Records identified by surface markers (CNeoIDList + NPfl).
    records_raw: list[dict] = []
    by_class: dict[int, int] = {}
    for rec in walk_records(c):
        d = {
            "page_id": rec.page_id,
            "abs_off": rec.abs_off,
            "len": len(rec.raw),
        }
        if rec.class_id is not None:
            d["class_id"] = rec.class_id
            d["class_name"] = (spec_for(rec.class_id).name
                               if spec_for(rec.class_id) else "?")
            by_class[rec.class_id] = by_class.get(rec.class_id, 0) + 1
        records_raw.append(d)

    (json_dir / "records.json").write_text(
        json.dumps({"records": records_raw}, indent=2), encoding="utf-8"
    )
    click.echo(f"wrote {json_dir / 'records.json'}  ({len(records_raw):,} records)")

    # 2. Class dump.
    cls = dump_classes(by_class)
    (json_dir / "classes.json").write_text(
        json.dumps(cls, indent=2), encoding="utf-8"
    )
    click.echo(f"wrote {json_dir / 'classes.json'}  ({cls['total_classes']} classes)")

    # 3. CNeoIDList full inventory.
    nl = dump_neoidlist(c)
    (json_dir / "neoidlist.json").write_text(
        json.dumps(nl, indent=2), encoding="utf-8"
    )
    click.echo(f"wrote {json_dir / 'neoidlist.json'}  ({nl['total']:,} ID records)")

    # 4. Production paths (scene → asset map).
    paths = dump_production_paths(c)
    (json_dir / "production_paths.json").write_text(
        json.dumps(paths, indent=2), encoding="utf-8"
    )
    click.echo(f"wrote {json_dir / 'production_paths.json'}  "
               f"({paths['total']:,} paths)")

    # 4b. All strings.
    strings = dump_strings(c)
    (json_dir / "strings.json").write_text(
        json.dumps(strings, indent=2), encoding="utf-8"
    )
    click.echo(f"wrote {json_dir / 'strings.json'}  ({strings['total']:,} strings)")

    # 4c. Page index.
    pidx = dump_page_index(c)
    (json_dir / "page_index.json").write_text(
        json.dumps(pidx, indent=2), encoding="utf-8"
    )
    click.echo(f"wrote {json_dir / 'page_index.json'}  ({pidx['total_pages']:,} pages)")

    # 4d. Decision tree (joining the format-analysis flow CSVs).
    try:
        from hdb_extract.ext import (
            load_scene_asset_map,
            load_variables_full,
            trigger_system_json,
        )
        from hdb_extract.extractors import (
            build_decision_tree,
            build_triggers_full,
            dump_xt_scripts,
            organize_xt_by_category,
            write_scenes_json,
        )

        dtree = build_decision_tree(c)
        (json_dir / "decision_tree.json").write_text(
            json.dumps(dtree, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )
        s = dtree["summary"]
        click.echo(f"wrote {json_dir / 'decision_tree.json'}  "
                   f"({s['scenes']} scenes, {s['hotspots']} hotspots, "
                   f"{s['actions']} actions, {s['triggers']} triggers, "
                   f"{s['variables']} vars, {s['conversations']} convs, "
                   f"{s['emails']} emails, {s['endings']} endings)")

        # 4d-bis. Trigger system grammar (D1).
        tsys = trigger_system_json()
        (json_dir / "trigger_system.json").write_text(
            json.dumps(tsys, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )
        click.echo(f"wrote {json_dir / 'trigger_system.json'}  "
                   f"({tsys['counts']['event_types']} events, "
                   f"{tsys['counts']['action_types']} actions, "
                   f"{tsys['counts']['expression_operators']} expr ops)")

        # 4d-4. Full variables (D5).
        vars_full = load_variables_full()
        (json_dir / "variables_full.json").write_text(
            json.dumps({
                "_source": "gam_variables.csv + gam_variables_semantics.csv",
                "total": len(vars_full),
                "variables": vars_full,
            }, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )
        click.echo(f"wrote {json_dir / 'variables_full.json'}  "
                   f"({len(vars_full)} variables enriched)")

        # 4d-5. Enriched triggers (D3).
        triggers_enriched = build_triggers_full(dtree["triggers"], vars_full)
        (json_dir / "triggers_full.json").write_text(
            json.dumps(triggers_enriched, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )
        click.echo(f"wrote {json_dir / 'triggers_full.json'}  "
                   f"({triggers_enriched['total']} byte-direct, "
                   f"inner records packed in HDB: G1 pending)")

        # 4e. Per-scene structured JSON with ordered flow + enriched triggers.
        scenes_dir = out_dir / "scenes"
        scene_asset_map = load_scene_asset_map()
        n_mapped = sum(len(v) for v in scene_asset_map.values())
        var_sem = {v["name"]: v for v in vars_full}
        idx = write_scenes_json(
            dtree, scenes_dir,
            scene_asset_map=scene_asset_map,
            var_semantics=var_sem,
            full_triggers=triggers_enriched["triggers"],
        )
        click.echo(f"wrote {scenes_dir}/*.json  "
                   f"(00_index + {idx['total_scenes']} scenes "
                   f"with {n_mapped:,} productions, "
                   f"flow ordered + triggers enriched)")

        # 4e-ter. Per-scene Markdown for human reading.
        from hdb_extract.extractors import write_scenes_md, write_storyboards
        n_md = write_scenes_md(scenes_dir)
        click.echo(f"wrote {scenes_dir}/*.md  ({n_md} scenes readable)")

        # 4e-quater. Narrative storyboard per scene.
        n_sb = write_storyboards(dtree, scenes_dir, scene_asset_map=scene_asset_map)
        click.echo(f"wrote {scenes_dir}/*_storyboard.md  "
                   f"({n_sb} narrative storyboards)")

        # 4e-bis. Decoded records via PackedHDBSerializer.
        from hdb_extract.extractors import extract_decoded_records, write_scene_nl_files
        decoded = extract_decoded_records(c)
        (json_dir / "decoded_records.json").write_text(
            json.dumps(decoded, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )
        click.echo(f"wrote {json_dir / 'decoded_records.json'}  "
                   f"({decoded['total_decoded']} records decoded, "
                   f"{decoded['total_no_match']} no-match, "
                   f"by_class={decoded['by_class']})")

        # 4e-ter. Typed records via marker_flag16 = class_id.
        from hdb_extract.extractors.typed_records import extract_typed_records
        typed = extract_typed_records(c)
        (json_dir / "typed_records.json").write_text(
            json.dumps(typed, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )
        click.echo(f"wrote {json_dir / 'typed_records.json'}  "
                   f"({typed['total_typed']} typed VC* records across "
                   f"{len(typed['by_class'])} classes, "
                   f"{typed['total_container']} container records)")

        # 4e-quater. Leaf-resolved records with target page strings.
        from hdb_extract.btree.leaf_resolver import build_resolved_catalog
        leaf_resolved = build_resolved_catalog(c)
        (json_dir / "leaf_resolved.json").write_text(
            json.dumps(leaf_resolved, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )
        click.echo(f"wrote {json_dir / 'leaf_resolved.json'}  "
                   f"({leaf_resolved['total_resolved']} leaf records resolved "
                   f"via value32 -> target page, "
                   f"{len(leaf_resolved['by_class_with_strings'])} classes "
                   f"with extracted strings)")

        # 4e-quinque. NeoLogic .nl script files per scene (code-format).
        n_nl = write_scene_nl_files(
            dtree, scenes_dir,
            scene_asset_map=scene_asset_map,
            var_semantics=var_sem,
            decoded_records=decoded["decoded_records"],
        )
        click.echo(f"wrote {scenes_dir}/*.nl  ({n_nl} NeoLogic scripts)")

        # 4f. XT scripts dump (D4).
        xt = dump_xt_scripts()
        if xt["total"] > 0:
            (json_dir / "xt_scripts.json").write_text(
                json.dumps(xt, indent=2, ensure_ascii=False),
                encoding="utf-8",
            )
            click.echo(f"wrote {json_dir / 'xt_scripts.json'}  "
                       f"({xt['total']} XT scripts, by_category="
                       f"{xt['by_category']})")
            xt_out_dir = out_dir / "xt" / "by_category"
            counts = organize_xt_by_category(out_dir=xt_out_dir, dump=xt)
            total_copied = sum(counts.values())
            click.echo(f"wrote {xt_out_dir}/<category>/*.txt  "
                       f"({total_copied} files organized)")
    except Exception as e:
        import traceback
        click.echo(f"decision_tree skipped : {e}", err=True)
        traceback.print_exc()

    # 5. Summary
    summary = {
        "hdb_path": str(hdb_path),
        "hdb_size_bytes": len(c.raw),
        "page_count": c.page_count,
        "trailer_size": c.trailer.size,
        "trailer_is_zero": c.trailer.is_zero_padding,
        "header": {
            "version": c.header.version,
            "record_count": c.header.record_count,
            "class_count": c.header.class_count,
            "root_or_size_hex": f"0x{c.header.root_or_size:08x}",
        },
        "extracted": {
            "records_surface": len(records_raw),
            "neoidlist_records": nl["total"],
            "production_paths": paths["total"],
            "classes_documented": cls["total_classes"],
        },
        "limitations": {
            "g1_packed_wire_unresolved": True,
            "g3_btree_internal_pending": True,
            "g5_nl_bytecode_pending": True,
        },
    }
    (json_dir / "summary.json").write_text(
        json.dumps(summary, indent=2), encoding="utf-8"
    )
    click.echo(f"wrote {json_dir / 'summary.json'}")

    # 6. SQLite cross-table.
    from hdb_extract.output import write_sqlite
    db_path = out_dir / "hdb_extract.db"
    write_sqlite(
        db_path,
        classes=cls,
        neoidlist=nl,
        production_paths=paths,
        records=records_raw,
        summary=summary,
    )
    click.echo(f"wrote {db_path}  (cross-table SQLite)")


@main.command(name="dump-complete")
@click.argument("hdb_path", type=click.Path(exists=True, path_type=Path))
@click.option("--out", "out_path", type=click.Path(path_type=Path),
              default="out/json/hdb_complete.json")
@click.option("--verify/--no-verify", default=True,
              help="Verify roundtrip from dump back to bytes")
def dump_complete_cmd(hdb_path: Path, out_path: Path, verify: bool):
    """Dump the HDB completely to JSON (every byte represented).

    Output is large (~100MB). Each page has hex content + ASCII runs +
    markers. Reversible : the dump can reconstruct the original bytes.
    """
    from hdb_extract.extractors import dump_complete, verify_dump
    c = HdbContainer.from_path(hdb_path)
    click.echo(f"Building byte-direct dump of {len(c.raw):,} bytes...")
    dump = dump_complete(c)
    click.echo(f"  pages : {len(dump['pages']):,}")
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(dump, indent=2), encoding="utf-8")
    click.echo(f"wrote {out_path}  ({out_path.stat().st_size:,} bytes)")
    if verify:
        ok = verify_dump(c, dump)
        click.echo(f"roundtrip from dump byte-identical : {ok}")
        sys.exit(0 if ok else 1)


@main.command(name="dump-class")
@click.argument("class_spec")
@click.argument("hdb_path", type=click.Path(exists=True, path_type=Path))
@click.option("--limit", type=int, default=5)
def dump_class(class_spec: str, hdb_path: Path, limit: int):
    """Dump first N instances of one class (by id or name)."""
    if class_spec.startswith("0x"):
        cid = int(class_spec, 16)
        spec = spec_for(cid)
    else:
        spec = CLASSES_BY_NAME.get(class_spec)
    if spec is None:
        click.echo(f"unknown class : {class_spec}", err=True)
        sys.exit(1)

    c = HdbContainer.from_path(hdb_path)
    found = 0
    for rec in walk_records(c):
        if found >= limit:
            break
        if rec.class_id != spec.class_id:
            continue
        # Try to decode body after the CNeoIDList header (best-effort).
        body = rec.raw[30:]
        ser = HDBSerializer(body, absolute_offset=rec.abs_off + 30)
        try:
            decoded = apply_read(ser, spec)
            click.echo(f"\n--- record @ page {rec.page_id} off {rec.record_off} ---")
            for k, v in decoded.items():
                click.echo(f"  {k} = {v!r}")
            found += 1
        except Exception as e:
            click.echo(f"decode error : {e}", err=True)


@main.command()
@click.argument("hdb_path", type=click.Path(exists=True, path_type=Path))
def verify(hdb_path: Path):
    """Verify container-level byte-identical roundtrip."""
    c = HdbContainer.from_path(hdb_path)
    ok = c.roundtrip_ok()
    click.echo(f"container roundtrip byte-identical : {ok}")
    sys.exit(0 if ok else 1)


@main.command()
@click.argument("pff_path", type=click.Path(exists=True, path_type=Path))
@click.option("--out", "out_dir", type=click.Path(path_type=Path),
              default=None, help="if set, extract every entry to this directory")
def pff(pff_path: Path, out_dir: Path | None):
    """Inspect or extract a NeoLogic .PFF archive (X.PFF, X7.PFF, ...)."""
    from hdb_extract.extractors.pff_extract import PffArchive, extract_pff_to_dir

    archive = PffArchive.from_path(pff_path)
    click.echo(f"file       : {pff_path}")
    click.echo(f"size       : {len(archive.raw):,} bytes")
    click.echo(f"entries    : {archive.entry_count}")
    click.echo(f"header     : {archive.header_size} bytes")
    click.echo(f"padding    : {archive.padding_size} bytes")
    click.echo(f"first entry @ 0x{archive.offsets[0]:08x}")
    click.echo(f"roundtrip byte-identical : {archive.roundtrip_ok()}")
    if out_dir is not None:
        n = extract_pff_to_dir(pff_path, out_dir)
        click.echo(f"extracted {n} entries to {out_dir}")


@main.command()
@click.argument("mov_path", type=click.Path(exists=True, path_type=Path))
@click.option("--out", "out_path", type=click.Path(path_type=Path),
              default=None, help="if set, copy the input to this path with a .mov extension")
def mov(mov_path: Path, out_path: Path | None):
    """Inspect a QuickTime .xmv/.nmv/.amv/.dmv video file."""
    from hdb_extract.extractors.mov_extract import MovFile, extract_to_mov

    m = MovFile.from_path(mov_path)
    s = m.summary()
    click.echo(f"file       : {mov_path}")
    click.echo(f"size       : {s['size']:,} bytes")
    click.echo(f"atoms      : {' '.join(s['top_level_atoms'])}")
    click.echo(f"codec      : {s['video_codec']}")
    click.echo(f"dimensions : {s['dimensions']}")
    click.echo(f"duration   : {s['duration_seconds']:.2f}s"
               if s['duration_seconds'] else "duration   : unknown")
    click.echo(f"frames     : {s['frame_count']}")
    if out_path is not None:
        written = extract_to_mov(mov_path, out_path)
        click.echo(f"copied to {written}")


@main.command(name="merge-locale")
@click.option("--tree", "tree_path", required=True,
              type=click.Path(exists=True, path_type=Path),
              help="path to decision_tree.json (from `hdb_extract extract`)")
@click.option("--dll", "dll_paths", multiple=True, required=True,
              type=click.Path(exists=True, path_type=Path),
              help="localization DLL (repeat for multiple)")
@click.option("--out", "out_path", required=True, type=click.Path(path_type=Path),
              help="output merged JSON path")
def merge_locale(tree_path: Path, dll_paths: tuple[Path, ...], out_path: Path):
    """Inject human-readable strings into decision_tree.json from XFiles*.dll."""
    from hdb_extract.extractors.merge_locale import merge_and_write

    stats = merge_and_write(tree_path, list(dll_paths), out_path)
    click.echo(f"loaded {stats['strings_loaded']:,} strings from {len(dll_paths)} DLL(s)")
    click.echo(f"resolved   : {stats['resolved']:,} text_id references")
    click.echo(f"unresolved : {stats['unresolved']:,} (not in DLL set)")
    click.echo(f"wrote {out_path}")


@main.command()
@click.argument("dll_path", type=click.Path(exists=True, path_type=Path))
@click.option("--out", "out_path", type=click.Path(path_type=Path),
              default=None, help="if set, write JSON {id: text} to this path")
def loc(dll_path: Path, out_path: Path | None):
    """Decode a localization DLL (XFilesc/e/s/t.dll) into a string table."""
    from hdb_extract.extractors.loc_extract import LocaleDLL, extract_locale_to_json

    loc_obj = LocaleDLL(dll_path)
    s = loc_obj.summary()
    click.echo(f"file       : {dll_path}")
    click.echo(f"category   : {s['category']}")
    click.echo(f"language   : {s['language_hex']} ({s['language_id']})")
    click.echo(f"strings    : {s['string_count']}")
    if s['id_range']:
        click.echo(f"id range   : {s['id_range'][0]}..{s['id_range'][1]}")
    if out_path is not None:
        extract_locale_to_json(dll_path, out_path)
        click.echo(f"wrote {out_path}")


@main.command(name="game-def")
@click.argument("hdb_path", type=click.Path(exists=True, path_type=Path))
@click.option("--out", "out_path", type=click.Path(path_type=Path),
              default="examples/outputs/game_definition.json",
              help="output path for the unified game_definition.json")
@click.option("--artifacts-dir", "artifacts_dir",
              type=click.Path(path_type=Path), default=None,
              help="directory holding the reference artifact JSONs "
                   "(default: OSS/examples/outputs)")
@click.option("--dll-dir", "dll_dir",
              type=click.Path(path_type=Path), default=None,
              help="directory holding the XFiles*.dll files (default: HDB dir)")
def game_def_cmd(hdb_path: Path, out_path: Path,
                 artifacts_dir: Path | None,
                 dll_dir: Path | None):
    """Emit the unified game_definition.json (schema v1)."""
    from hdb_extract.extractors.game_definition import (
        verify_certainty, write_game_definition,
    )

    gd = write_game_definition(hdb_path, out_path,
                               artifacts_dir=artifacts_dir,
                               dll_dir=dll_dir)
    audit = verify_certainty(gd)
    fc = gd["_meta"]["field_counts"]
    click.echo(f"wrote {out_path}")
    click.echo(f"  size      : {Path(out_path).stat().st_size:,} bytes")
    click.echo(f"  fields    : total={fc['total']['value']:,}  "
               f"byte-direct={fc['byte_direct']['value']:,}  "
               f"undetermined={fc['undetermined']['value']:,}  "
               f"walkthrough={fc['walkthrough']['value']:,}")
    click.echo(f"  byte_direct_pct : "
               f"{gd['_meta']['byte_direct_pct']['value']}%")
    click.echo(f"  certainty audit : ok={audit['ok']}")
    if not audit["ok"]:
        for path, c in audit["violations"][:20]:
            click.echo(f"    VIOLATION : {path}  ->  {c}", err=True)
        sys.exit(1)


@main.command()
@click.argument("xv_dir", type=click.Path(exists=True, file_okay=False,
                                            path_type=Path))
@click.option("--out", "out_path", type=click.Path(path_type=Path),
              default="examples/outputs/hotspots_inventory.json",
              help="output path for the hotspots inventory JSON")
def hotspots(xv_dir: Path, out_path: Path):
    """Decode every HSPT file under XV_DIR into a single inventory JSON."""
    from hdb_extract.extractors.hotspots import write_hotspots_inventory

    stats = write_hotspots_inventory(xv_dir, out_path)
    click.echo(f"wrote {out_path}")
    for k in ("scenes_total", "rects_total", "action_ids_distinct",
              "skipped_files"):
        click.echo(f"  {k:22s}: {stats[k]:,}")
    rng = stats["action_id_range"]
    click.echo(f"  action_id_range       : {rng['min']} .. {rng['max']}")


@main.command()
@click.argument("game_def_path", type=click.Path(exists=True,
                                                   path_type=Path))
@click.option("--out", "out_path", type=click.Path(path_type=Path),
              default="examples/outputs/playthrough_trace.json",
              help="output path for the playthrough trace JSON")
def trace(game_def_path: Path, out_path: Path):
    """Emit a per-step trace cross-referencing the unified game model."""
    from hdb_extract.extractors.playthrough_trace import write_playthrough_trace

    stats = write_playthrough_trace(game_def_path, out_path)
    click.echo(f"wrote {out_path}")
    for k, v in stats.items():
        click.echo(f"  {k:30s}: {v:,}")


@main.command(name="scene-map")
@click.argument("hdb_path", type=click.Path(exists=True, path_type=Path))
@click.option("--out", "out_path", type=click.Path(path_type=Path),
              default="examples/outputs/scene_asset_map.json",
              help="output path for the scene asset map JSON")
def scene_map_cmd(hdb_path: Path, out_path: Path):
    """Extract the byte-direct map from inline labels to XV/XN ids."""
    from hdb_extract.extractors.scene_asset_map import write_scene_asset_map

    stats = write_scene_asset_map(hdb_path, out_path)
    click.echo(f"wrote {out_path}")
    click.echo(f"  entries_total       : {stats['entries_total']:,}")
    click.echo(f"  distinct_locations  : {stats['distinct_locations']}")
    click.echo(f"  by_kind             : {stats['by_kind']}")
    click.echo(f"  by_asset_dir        : {stats['by_asset_dir']}")


@main.command()
@click.argument("game_def_path", type=click.Path(exists=True,
                                                   path_type=Path))
@click.option("--out", "out_path", type=click.Path(path_type=Path),
              default="examples/outputs/dispatcher_report.json",
              help="output path for the dispatcher JSON report")
def dispatch(game_def_path: Path, out_path: Path):
    """Replay the 29-step flow over the unified model and mutate variables."""
    from hdb_extract.extractors.dispatcher import write_dispatch_report

    counts = write_dispatch_report(game_def_path, out_path)
    click.echo(f"wrote {out_path}")
    for k, v in counts.items():
        click.echo(f"  {k:25s}: {v}")


@main.command()
@click.argument("gam_path", type=click.Path(exists=True, path_type=Path))
@click.option("--out", "out_path", type=click.Path(path_type=Path),
              default=None, help="if set, write a JSON summary to this path")
def gam(gam_path: Path, out_path: Path | None):
    """Inspect or summarise an XFILES.GAM savegame (same container as HDB)."""
    from hdb_extract.extractors.gam_extract import GamFile, extract_gam_to_json

    f = GamFile.from_path(gam_path)
    s = f.summary()
    click.echo(f"file       : {gam_path}")
    click.echo(f"size       : {s['file_size']:,} bytes")
    click.echo(f"version    : {s['header']['version']}  "
               f"(GAM=5, HDB=3 — this file is {'GAM' if f.is_gam() else 'NOT a GAM'})")
    click.echo(f"record_count : {s['header']['record_count']:,}")
    click.echo(f"class_count  : {s['header']['class_count']}")
    click.echo(f"pages        : {s['pages']}")
    click.echo(f"trailer      : {s['trailer']['size']}B  "
               f"({'zero-padded' if s['trailer']['is_zero_padding'] else 'non-zero'})")
    click.echo(f"roundtrip byte-identical : {s['roundtrip_byte_identical']}")
    if out_path is not None:
        extract_gam_to_json(gam_path, out_path)
        click.echo(f"wrote {out_path}")


if __name__ == "__main__":
    main()
