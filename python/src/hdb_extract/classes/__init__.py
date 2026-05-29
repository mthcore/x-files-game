"""52 VC* classes — data-driven Read specifications.

Each `ClassSpec` describes the sequence of `Op` invocations a
`VC*::Read` method performs against the HDBSerializer. The
`apply_read(serializer, spec)` driver consumes that DSL and produces a
populated dict of fields for the class instance.

The full set mirrors the C++ reference decoder (see `cpp/`), established
byte-direct from the on-disk record structure.
See `generated/all_classes.py` for the data.
"""
from hdb_extract.classes.spec import ClassSpec, Op
from hdb_extract.classes.registry import (
    CLASSES_BY_ID,
    CLASSES_BY_NAME,
    apply_read,
    spec_for,
)

__all__ = [
    "ClassSpec",
    "Op",
    "CLASSES_BY_ID",
    "CLASSES_BY_NAME",
    "apply_read",
    "spec_for",
]
