# Elastic Hashing

A C++20 header-only map based on the batch structure in
[Optimal Bounds for Open Addressing Without Reordering](https://arxiv.org/html/2501.02305).
The table uses geometrically decreasing levels, elastic insertion batches, and a
dyadic probe scheduler optimized for successful lookups.

```cpp
#include "elastic_hash.hpp"
#include <string>

int main() {
    ElasticHashTable<std::string, int> map;
    map.insert({"apple", 1});
    map["banana"] = 2;
    map.try_emplace("pear", 3);
    map.erase("apple");
    const auto& view = map;
    for (const auto& [key, value] : view) {
        // key and value are const through this view.
    }
}
```

## Behavior

* Keys are unique across all levels. Duplicate insertion returns the existing
  element and `false`, without changing its value or growing the table.
* Erasure leaves tombstones, so colliding keys remain reachable. Both `erase`
  overloads return the next physical iterator; erasing a missing key returns `end()`.
* Lookup is bounded even when a level is full. Each level has a finite probe
  stream and a recorded maximum insertion probe index.
* `reserve`, automatic growth, and explicit `rehash` preserve the hash/equality
  objects and configured maximum load factor. `max_load_factor` accepts finite
  values in `(0, 1]`, rebuilding existing batches when changed. At least one slot
  remains free even when the requested maximum is 1.
* Const iteration, mutable-to-const iterator conversion, `size`, `empty`, `clear`,
  copy/move operations, and move-only mapped values are supported.
* `try_emplace` constructs the mapped value only when the key is absent.

Insertions can rebuild for growth or tombstone cleanup; rebuilding invalidates
all iterators and references. Erase invalidates only the erased element. This is
not a complete `std::unordered_map` replacement: it has no allocator interface,
node handles, or stable references across rehash. Equal keys must hash equally.
Copyable entries are copied into a replacement table before committing a rebuild;
for move-only entries, a throwing move/hash during rebuild can leave mapped
values moved-from. The implementation currently requires a compiler supporting
128-bit integer multiplication, such as GCC or Clang on 64-bit Linux.

## Performance

Lookup handles the first probe before setting up scheduler state, visits only
levels containing live entries, uses byte fingerprints to reduce key comparisons,
and uses a conservative membership filter to reject most absent keys. The filter
adds approximately one byte per bucket; fingerprints add another byte per bucket.
Capacity thresholds are cached. Rehash bypasses duplicate checks because the
source table already contains unique keys.

The [benchmark report](verification/BENCHMARKS.md) includes before/after timings
and the workload. Results are measurements of this implementation on one machine,
not universal performance guarantees.

## Complexity and verification

The paper proves `O(1)` amortized expected successful-search probe complexity and
`O(log(1/delta))` worst-case expected probe complexity in its insertion-only random
probe model. This practical container also supports deletion, growth, duplicate
checks, deterministic hashing, and a bounded fallback. **The paper's probabilistic
bounds are not established for those extensions or for arbitrary hash functions.**

[The Lean proof](verification/Scheduler.lean) establishes the scheduler's
`rank <= 16*i*j²` bound for every positive probe pair, plus counting, fallback
injectivity, and conditional weighted-cost bounds. It does not prove that the
C++ implementation satisfies the paper's random-probe assumptions or batch
probability estimates. [Verification scope and instructions](verification/README.md)
spell out exactly what is proved and what is tested.

## Build and check

```sh
bazelisk test -c opt //:elastic_hash_test //:elastic_hash_regression_test
bazelisk run -c opt //:unordered_map_benchmark
./verification/check.sh  # Lean 4.24.0 and a C++20 compiler; no extra libraries
```

The standalone regression suite can also run without Bazel or Lean:

```sh
c++ -std=c++20 -O2 -I. elastic_hash_regression_test.cc -o /tmp/elastic-regressions
/tmp/elastic-regressions
```
