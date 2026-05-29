# PyInstaller spec for building a single-binary `hdb-extract` CLI.
#
# Usage:
#   pip install pyinstaller pefile
#   pyinstaller hdb_extract.spec
#
# Output:
#   dist/hdb-extract            (Linux/macOS)
#   dist/hdb-extract.exe        (Windows)
#
# The executable is self-contained: no Python install required on the target
# machine. Per-OS binaries are produced automatically by
# .github/workflows/release.yml when a vX.Y.Z tag is pushed.

# -*- mode: python ; coding: utf-8 -*-
from pathlib import Path

block_cipher = None

a = Analysis(
    ['src/hdb_extract/__main__.py'],
    pathex=['src'],
    binaries=[],
    datas=[],
    hiddenimports=[
        'hdb_extract',
        'hdb_extract.cli',
        'hdb_extract.container',
        'hdb_extract.container.container',
        'hdb_extract.container.header',
        'hdb_extract.container.pages',
        'hdb_extract.container.trailer',
        'hdb_extract.classes',
        'hdb_extract.classes.registry',
        'hdb_extract.classes.spec',
        'hdb_extract.classes.generated.all_classes',
        'hdb_extract.btree',
        'hdb_extract.btree.walker',
        'hdb_extract.btree.leaf',
        'hdb_extract.btree.master_resolver',
        'hdb_extract.btree.follow_pointers',
        'hdb_extract.btree.leaf_resolver',
        'hdb_extract.serializer',
        'hdb_extract.serializer.cursor',
        'hdb_extract.serializer.tracking',
        'hdb_extract.serializer.neo_stream',
        'hdb_extract.serializer.packed',
        'hdb_extract.serializer.writer',
        'hdb_extract.extractors',
        'hdb_extract.extractors.pff_extract',
        'hdb_extract.extractors.gam_extract',
        'hdb_extract.extractors.loc_extract',
        'hdb_extract.extractors.mov_extract',
        'hdb_extract.extractors.merge_locale',
        'hdb_extract.format',
        'hdb_extract.format.fourcc',
        'hdb_extract.format.neoidlist',
        'hdb_extract.polymorphe',
        'hdb_extract.polymorphe.dispatcher',
        'hdb_extract.output',
        'hdb_extract.output.sqlite_writer',
        'hdb_extract.ext',
        'hdb_extract.verify',
        'pefile',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)
pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='hdb-extract',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=True,
    disable_windowed_traceback=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
