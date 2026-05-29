"""B-tree page walking.

Conservative implementation : currently walks the 202 leaf pages (0xC3)
linearly, which is enough to enumerate the records stored in leaves.

Full root → internal → leaf walking requires G3 (internal page layout)
to be fully decoded. Once G3 is done, `walk_records` should be updated
to follow the B-tree from `header.root_or_size`.
"""
from hdb_extract.btree.leaf import LeafRecord, iter_records_in_leaf
from hdb_extract.btree.walker import iter_leaf_pages, walk_records

__all__ = [
    "LeafRecord",
    "iter_records_in_leaf",
    "iter_leaf_pages",
    "walk_records",
]
