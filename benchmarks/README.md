# Comparing hash tables

[Read the measured comparison and conclusions](COMPARISON.md), and the
[million-entry results and optimization experiments](LARGE_INPUTS.md).
The copy reduction is now integrated; see the [high-occupancy follow-up](HIGH_OCCUPANCY.md).

This benchmark compares the current `ElasticHashTable` with:

| CSV name | Container | Version |
| :--- | :--- | :--- |
| `std` | `std::unordered_map` | libstdc++, recorded by `--info` |
| `absl` | `absl::flat_hash_map` | [Abseil 20260817.0](https://github.com/abseil/abseil-cpp/releases/tag/20260817.0) |
| `dense` | `ankerl::unordered_dense::map` | [unordered_dense 4.8.1](https://github.com/martinus/unordered_dense/releases/tag/v4.8.1) |
| `gnu_gp` | `__gnu_pbds::gp_hash_table` | libstdc++ |
| `gnu_cc` | `__gnu_pbds::cc_hash_table` | libstdc++ |
| `elastic` | `ElasticHashTable` | header SHA-256 recorded with results |

The CMake project pins dependency archives and checks their SHA-256 hashes.
It does not change the container implementation or the existing Bazel targets.
The harness requires Linux/glibc and GCC's libstdc++ headers, including when
compiled with Clang. Dependency licenses remain in their fetched source trees.

## Build and run

```sh
cmake -S benchmarks -B /tmp/elastic-comparison-build -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/elastic-comparison-build --target hash_comparison -j 4
python3 benchmarks/run.py /tmp/elastic-comparison-build/hash_comparison \
  --output /tmp/elastic-comparison-results
```

Python uses only its standard library. The runner pins itself and its serial
child processes to one CPU, randomizes configuration order with a fixed seed,
and retains every timing sample. It refuses to overwrite an existing report.
Use `--cpu`, `--sizes`, `--samples`, `--maps`, `--key-types`, and `--hash-modes`
to select workloads. `--operations` can select individual operations for large
working sets, and `--timeout` sets the per-configuration time limit. A short
validation run is:

```sh
python3 benchmarks/run.py /tmp/elastic-comparison-build/hash_comparison \
  --output /tmp/elastic-comparison-smoke --sizes 128 --samples 1 --target-ms 1
```

To run one configuration directly:

```sh
/tmp/elastic-comparison-build/hash_comparison elastic u64 native 45000 7 10
```

## Workloads

The default sizes are 4,096, 45,000 and 180,000 entries. Using sizes below
power-of-two growth thresholds avoids examining only recently doubled flat
tables. Every map keeps its own default maximum load and growth policy. **The
maps are not forced to the same actual load or memory budget**; those quantities
are reported so that the difference is visible.

Keys are distinct 64-bit random-looking integers from an invertible mixer, or
24-character strings derived injectively from those integers. String keys exceed
libstdc++'s short-string buffer. Insertion order and query order use separate
fixed-seed shuffles. Missing keys come from a disjoint range of the mixer input.
Input generation and string formatting are outside timed regions.

There are two hash configurations:

* `native`: the library's native hash for Abseil and unordered_dense;
  `std::hash` for std, ElasticHashTable, and both PBDS maps.
* `shared`: every container is given the same unordered_dense hasher through
  `SharedHash`, including its avalanche marker. Internal table-specific mixing, probing,
  and hash caching still differ. This controls the caller-supplied hash function,
  rather than claiming to make every table's hashing path identical.

Each process benchmarks one container/key/hash/size combination. Within it:

* `insert_grow`: insert distinct keys into a default-constructed map, including
  growth during insertion. Initial construction and final destruction are excluded.
* `insert_reserved`: reserve first, then time insertions. Reservation is excluded.
  PBDS is configured with external size access enabled and receives `resize(2*n)`
  for its default 0.5 growth threshold; other maps receive `reserve(n)`.
* `hit`, `miss`, `mixed`: read a populated table in shuffled query order, with
  100%, 0%, and 50% hit rates. The mixed query stream alternates hits and misses.
* `iterate`: sum mapped values over the whole map. No key strings are copied.
* `erase`: erase every existing key; fresh table preparation is excluded.
* `churn`: erase an existing key, then insert a distinct missing key, repeatedly
  until the original key set is replaced. Each erase/insert pair counts as **two**
  map operations. Tombstone cleanup and rehashing during the loop are included.

Read operations calibrate repeated passes to approximately `--target-ms` per
sample, with at least one pass. This measures warmed, repeated queries rather
than cold-cache latency. Mutation samples each perform one complete pass and may
last less than the target duration. The first insertion/mutation sample is an
untimed warmup; its result is still checked.

Every fixture validates its size, checksum and each key/value lookup. Read
samples check their expected sums; erase and churn check final sizes/checksums.
Compiler memory barriers prevent repeated query passes from being hoisted or
eliminated. Counters are printed so that work remains observable.

## Memory and interpretation

The `memory` row estimates incremental **live glibc allocator bytes per entry**
using `mallinfo2().uordblks + mallinfo2().hblkhd` before and after building a map.
The input/query arrays already exist. It includes map-owned string allocations,
allocation rounding, and some allocator cache effects. It excludes
the stack-resident map object, input arrays, code, and process overhead; it is
**not RSS or peak memory**. Allocator caching makes small differences approximate.
This measurement precedes throughput warmups in each fresh process and adds no
allocation instrumentation to the timed code.

Summary CSVs contain medians, selected sample quartiles, ranges, actual bucket
counts, and time relative to std (below 1 means faster). Medians come from seven
samples by default. A single shuffled seed, one query pattern, and one machine
do not cover all applications. Process scheduling, CPU frequency, cache state,
and per-instance randomized hashing can affect results; sample ranges expose
some of this variation. These measurements do not prove asymptotic complexity.

`results/` contains recorded runs, including raw data and environment metadata.
The previous implementation's comparison is kept separately in
[`verification/BENCHMARKS.md`](../verification/BENCHMARKS.md).

## Million-entry runs and copy-reduction experiment

A focused large run avoids the cost of benchmarking every mutation workload:

```sh
python3 benchmarks/run.py /tmp/elastic-comparison-build/hash_comparison \
  --output /tmp/elastic-large-u64 --sizes 1000000 3000000 --samples 5 \
  --maps std absl dense elastic --key-types u64 --hash-modes native \
  --operations insert_grow hit miss mixed
```

`experiments/make_copy_candidate.py` reproduces the copy-reduction experiment.
The change is now in production, so generating from the current header is
idempotent. Use `--source` with the historical header to recreate the original
before/after patch. The script writes a separate output file. Configure the optional second executable with:

```sh
python3 benchmarks/experiments/make_copy_candidate.py \
  /tmp/elastic-copy-candidate/elastic_hash.hpp \
  --patch /tmp/avoid_key_copies.patch
cmake -S benchmarks -B /tmp/elastic-comparison-build \
  -DELASTIC_CANDIDATE_HEADER=/tmp/elastic-copy-candidate/elastic_hash.hpp
cmake --build /tmp/elastic-comparison-build \
  --target hash_comparison hash_comparison_candidate -j 4
python3 benchmarks/run.py /tmp/elastic-comparison-build/hash_comparison_candidate \
  --elastic-header /tmp/elastic-copy-candidate/elastic_hash.hpp \
  --output /tmp/elastic-copy-results --maps elastic --key-types string \
  --hash-modes native --sizes 45000 1000000 --samples 5 \
  --operations insert_grow insert_reserved hit miss
```

The runner records the actual alternate header's hash using `--elastic-header`
and checks that both the binary and header remain unchanged during collection.
Finish all compilation before starting timing runs.

`experiments/copy_probe.cc` checks duplicate insertion using an aliased entry,
failed-rebuild behavior, and mapped arguments that alias a table during growth.
It prints observed key-copy counts for a single insertion and rebuild; these
counts are experimental observations, rather than API guarantees.

`experiments/profile_probes.py` builds a temporary header with a probe counter
and runs 100,000 hit/miss queries from the exact integer benchmark dataset. This
measures logical bucket probes, not cache misses or wall time. It compiles and
runs separately from the timing collection:

```sh
python3 benchmarks/experiments/profile_probes.py --sizes 1000000 3000000
```

## High occupancy

```sh
python3 benchmarks/run_occupancy.py /tmp/elastic-comparison-build/hash_comparison \
  --output /tmp/elastic-high-occupancy --buckets 262143 \
  --loads 0.75 0.90 0.95 0.99 0.999 --samples 5
```

For each requested load `L`, the campaign inserts `floor(reference_buckets * L)`
keys. Every fixture sets `max_load_factor(L)` and calls `reserve(n)` before
measurement. Insertion timing is `insert_reserved`; reservation and policy
configuration are excluded. The harness checks that insertion does not grow the
preallocated bucket array. The memory measurement includes preparation.

Reference buckets describe the intended ElasticHashTable capacity; they are
**not an imposed capacity for other implementations**. Actual bucket counts,
occupancy, requested maximum, and returned `max_load_factor()` are recorded.
Std uses prime bucket counts. Unordered_dense uses powers of two. Abseil's
`max_load_factor(float)` setter does nothing (its getter returns 1), and its own
growth policy reserves more buckets at high requested loads. Therefore a 99%
request is not a claim that all four tables achieved 99% occupancy. See the
actual occupancy column and heap-per-key measurements.

GNU PBDS tables are excluded from this protocol because their resize-policy
API differs. Native-hash defaults are used unless `--hash-modes shared` is
selected. Large campaigns can select just integer keys:

```sh
python3 benchmarks/run_occupancy.py /tmp/elastic-comparison-build/hash_comparison \
  --output /tmp/elastic-high-large --buckets 1048575 --loads 0.95 0.99 \
  --key-types u64 --samples 5
```

For a historical baseline executable, use `--elastic-header` to record the
actual header compiled into it; the optional CMake alternate-header target can
compile such a baseline using the same harness and flags.

## Alternating optimization comparison

To compare the current header with the recorded pre-copy baseline using the
same harness, first build the alternate-header executable:

```sh
git show 75a09af:elastic_hash.hpp > /tmp/elastic-before-copy.hpp
cmake -S benchmarks -B /tmp/elastic-comparison-build \
  -DELASTIC_CANDIDATE_HEADER=/tmp/elastic-before-copy.hpp
cmake --build /tmp/elastic-comparison-build \
  --target hash_comparison hash_comparison_candidate -j 4
python3 benchmarks/experiments/compare_copy.py \
  /tmp/elastic-comparison-build/hash_comparison \
  /tmp/elastic-comparison-build/hash_comparison_candidate \
  --baseline-header /tmp/elastic-before-copy.hpp \
  --output /tmp/elastic-copy-paired --pairs 5 --samples 5
```

Here the executable named `candidate` intentionally contains the **historical
baseline**; `ELASTIC_CANDIDATE_HEADER` supports any alternate header. This runner
alternates variant order, records process CPU/wall times and individual samples,
and summarizes paired time ratios. The measured workload is reserved insertion
of strings at 75% occupancy, with no timed rehash. Copy/exception/aliasing
regressions validate the rebuild optimization separately.
