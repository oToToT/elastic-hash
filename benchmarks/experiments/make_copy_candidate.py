#!/usr/bin/env python3
"""Generate the copy-elision prototype without changing the production header."""
from pathlib import Path
import argparse
import difflib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path)
parser.add_argument('--patch', type=Path)
parser.add_argument('--source', type=Path, help='optional historical, pre-optimization header')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
original = (args.source or root/'elastic_hash.hpp').read_text()
changes = [
    ('''    // Stage before a possible rebuild, including when value aliases this table.
    value_type staged(std::move(value));
    prepare_insert();
    return {iterator(this, insert_absent(std::move(staged), hash)), true};''',
     '''    // A value aliasing a live table entry was already found above. An absent
    // entry can be forwarded directly, avoiding another copy of its const key.
    prepare_insert();
    return {iterator(this, insert_absent(std::move(value), hash)), true};'''),
    ('''  std::size_t insert_absent(value_type &&value, std::size_t hash) {''',
     '''  template <class V>
  std::size_t insert_absent(V &&value, std::size_t hash) {'''),
    ('''        data_[bucket].emplace(std::move(value));''',
     '''        data_[bucket].emplace(std::forward<V>(value));'''),
    ('''        value_type value(*bucket);
        replacement.insert_absent(std::move(value), hash);''',
     '''        replacement.insert_absent(*bucket, hash);'''),
]
candidate = original
for before, after in changes:
    if candidate.count(before) == 1:
        candidate = candidate.replace(before, after, 1)
    elif candidate.count(before) == 0 and candidate.count(after) == 1:
        pass  # The change has already been promoted to this header.
    else:
        raise SystemExit('source has changed: cannot identify the copy optimization uniquely')
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(candidate)
if args.patch:
    args.patch.write_text(''.join(difflib.unified_diff(
        original.splitlines(keepends=True), candidate.splitlines(keepends=True),
        fromfile='a/elastic_hash.hpp', tofile='b/elastic_hash.hpp')))
print(args.output)
