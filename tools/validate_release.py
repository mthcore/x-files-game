#!/usr/bin/env python3
"""validate_release.py — Sanity check the OSS repository structure.

Run this before tagging a release. Exits non-zero if any required file is
missing or any size threshold is exceeded.
"""
from __future__ import annotations

import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

REQUIRED_FILES = [
    # Top-level legal / meta
    "README.md",
    "LICENSE",
    "NOTICE",
    "CONTRIBUTING.md",
    "SECURITY.md",
    "CHANGELOG.md",
    ".gitignore",
    ".gitattributes",
    # Python package
    "python/pyproject.toml",
    "python/README.md",
    "python/src/hdb_extract/__init__.py",
    "python/src/hdb_extract/cli.py",
    "python/src/hdb_extract/classes/generated/all_classes.py",
    "python/src/hdb_extract/extractors/pff_extract.py",
    "python/src/hdb_extract/extractors/gam_extract.py",
    "python/src/hdb_extract/extractors/loc_extract.py",
    "python/src/hdb_extract/extractors/mov_extract.py",
    "python/tests/unit/test_classes_apply_read.py",
    "python/tests/unit/test_pff_extract.py",
    "python/tests/unit/test_gam_extract.py",
    "python/tests/unit/test_loc_extract.py",
    "python/tests/unit/test_mov_extract.py",
    # C++ package
    "cpp/CMakeLists.txt",
    "cpp/README.md",
    "cpp/src/tools/hdb_dump.cpp",
    "cpp/include/hdb/hdb_container.h",
    "cpp/include/assets/cinepak_decoder.h",
    "cpp/include/assets/ima_adpcm.h",
    "cpp/include/assets/qt_container.h",
    "cpp/third_party/catch2/catch_amalgamated.hpp",
    "cpp/third_party/catch2/catch_amalgamated.cpp",
    # Reference headers (third-party, non-redistributable game files excluded)
    "headers/neopersist_3.0/NOTICE.md",
    "headers/neopersist_3.0/CNeoPersist.h",
    "headers/neopersist_3.0/CNeoStream.h",
    "headers/neopersist_3.0/CNeoStream.cp",
    "headers/neopersist_3.0/NeoTypes.h",
    # Example outputs
    "examples/README.md",
    "examples/outputs/triggers_full.json",
    "examples/outputs/decision_tree.json",
    "examples/outputs/classes_summary.json",
    "examples/outputs/hdb_extract.sample.db",
    "examples/coverage_final.md",
    # Documentation
    "docs/QUICKSTART.md",
    "docs/FORMAT.md",
    "docs/HISTORY.md",
    "docs/WIRE_FORMAT.md",
    "docs/CLASSES.md",
    "docs/TRIGGERS.md",
    "docs/HOTSPOTS.md",
    "docs/DIALOGUES.md",
    "docs/DECISIONS.md",
    "docs/NEOPERSIST.md",
    "docs/GLOSSARY.md",
    "docs/ROADMAP.md",
    # v0.2 additions
    "docs/ISO_FILES.md",
    "docs/PFF_FORMAT.md",
    "docs/GAM_FORMAT.md",
    "docs/LOCALIZATION.md",
    "docs/VIDEO.md",
    "docs/XT_SCRIPTS.md",
    "docs/reference/hdb_btree_internal.md",
    "docs/reference/trigger_system_official.md",
    "docs/reference/W23_4_hdb_tlv_per_class.md",
    # v0.3 additions
    "python/src/hdb_extract/extractors/merge_locale.py",
    "python/src/hdb_extract/serializer/writer.py",
    "python/tests/unit/test_merge_locale.py",
    "python/tests/unit/test_writer_roundtrip.py",
    "python/hdb_extract.spec",
    "cpp/include/vc/_size_assert.h",
    "cpp/include/assets/pict_decoder.h",
    "cpp/third_party/stb/stb_image.h",
    "mkdocs.yml",
    "docs/index.md",
    # CI
    ".github/workflows/python.yml",
    ".github/workflows/cpp.yml",
    ".github/workflows/release.yml",
    ".github/workflows/pages.yml",
]

FORBIDDEN_PATTERNS = [
    # Never commit the game binary
    "XFILES.HDB",
    "Xfiles.exe",
]

MAX_FILE_SIZES = {
    # path glob (suffix match) -> max bytes
    "examples/outputs/decision_tree.json": 5 * 1024 * 1024,  # 5 MB cap
    "examples/outputs/hdb_extract.sample.db": 5 * 1024 * 1024,
}

MAX_REPO_SIZE = 200 * 1024 * 1024  # 200 MB hard cap


def fail(msg: str) -> None:
    print(f"FAIL: {msg}", file=sys.stderr)


def warn(msg: str) -> None:
    print(f"WARN: {msg}", file=sys.stderr)


def main() -> int:
    errors = 0

    # 1. Required files
    for rel in REQUIRED_FILES:
        if not (REPO / rel).is_file():
            fail(f"missing required file: {rel}")
            errors += 1

    # 2. Forbidden patterns
    for pat in FORBIDDEN_PATTERNS:
        hits = list(REPO.rglob(pat))
        if hits:
            for h in hits:
                fail(f"forbidden file present: {h.relative_to(REPO)}")
                errors += 1

    # 3. File size caps
    for rel, cap in MAX_FILE_SIZES.items():
        p = REPO / rel
        if p.is_file() and p.stat().st_size > cap:
            fail(f"oversize: {rel} = {p.stat().st_size:,}B > cap {cap:,}B")
            errors += 1

    # 4. Total repo size
    total = 0
    for p in REPO.rglob("*"):
        if p.is_file() and ".git" not in p.parts and "__pycache__" not in p.parts:
            total += p.stat().st_size
    if total > MAX_REPO_SIZE:
        fail(f"repo too large: {total:,}B > cap {MAX_REPO_SIZE:,}B")
        errors += 1
    else:
        print(f"repo size: {total:,} bytes  (cap {MAX_REPO_SIZE:,})")

    # 5. Python tests collect cleanly (smoke check)
    py_tests = REPO / "python" / "tests"
    if py_tests.is_dir():
        n_tests = len(list(py_tests.rglob("test_*.py")))
        print(f"python test files: {n_tests}")
        if n_tests < 5:
            warn(f"only {n_tests} test files — expected 5+")

    if errors:
        print(f"\nvalidate_release: {errors} ERROR(S)")
        return 1
    print("\nvalidate_release: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
