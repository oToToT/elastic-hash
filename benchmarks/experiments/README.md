# Copy reduction and logical-probe experiments

The copy-reduction experiment has now been promoted into `elastic_hash.hpp`.
The notes below retain the historical before/after experiment. Recorded timings
identify the compiled header and executable through their SHA-256 hashes.

## Avoiding redundant const-key copies

`avoid_key_copies.patch` records the change already integrated into production.
To reproduce the historical candidate from its original baseline:

```sh
git show 75a09af:elastic_hash.hpp > /tmp/elastic-before-copy.hpp
python3 benchmarks/experiments/make_copy_candidate.py \
  /tmp/elastic-copy-candidate/elastic_hash.hpp \
  --source /tmp/elastic-before-copy.hpp --patch /tmp/avoid_key_copies.patch
```

The change removes the extra temporary in rvalue insertion and forwards entries
directly into destination slots during rebuilds. `value_type` has a const key,
so moving a pair still copies its key. A temporary pair therefore introduces an
extra string allocation/copy even when its mapped value moves cheaply.

The aliasing argument is specific: a valid input pair that aliases a live table
entry has its existing key found by the duplicate check before any rebuilding.
An input pair for an absent key can be forwarded after `prepare_insert`.
`try_emplace` continues staging its arguments before growth because a mapped
argument can refer to another entry's storage. Copyable entries are copied into
the replacement table before swapping, retaining the failed-rebuild guarantee.
Move-only mapped values keep the existing move path and documented exception
limitations.

No probe function, insertion level, metadata policy, lookup scheduler, or table
capacity rule changes. The scheduler proof therefore models the same sequence.
This experiment does not establish the paper's probabilistic hash assumptions.

`copy_probe.cc` was run against both headers. For a single entry, excluding the
caller's construction of the input pair, the observations were:

| Operation | Before optimization | Integrated optimization |
| :--- | ---: | ---: |
| Rvalue insert without growth | 2 | 1 |
| Rebuild, per existing entry | 2 | 1 |

Both runs passed aliased duplicate insertion, failure during rebuild, and
`try_emplace` with an argument referring to an existing mapped value during
growth. The candidate also passed the full standalone regression suite under
AddressSanitizer and UndefinedBehaviorSanitizer (`detect_leaks=0` because the
execution environment runs under a tracer).

Example commands for reproducing the focused checks:

```sh
clang++ -std=c++20 -O2 -I. \
  '-DELASTIC_HASH_HEADER="/tmp/elastic-before-copy.hpp"' \
  benchmarks/experiments/copy_probe.cc -o /tmp/copy-probe-before
/tmp/copy-probe-before
clang++ -std=c++20 -O2 -I. benchmarks/experiments/copy_probe.cc \
  -o /tmp/copy-probe-production
/tmp/copy-probe-production
clang++ -std=c++20 -O2 -I. \
  '-DELASTIC_HASH_HEADER="/tmp/elastic-copy-candidate/elastic_hash.hpp"' \
  benchmarks/experiments/copy_probe.cc -o /tmp/copy-probe-candidate
/tmp/copy-probe-candidate
cp elastic_hash_regression_test.cc /tmp/elastic-copy-candidate/regressions.cc
clang++ -std=c++20 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer /tmp/elastic-copy-candidate/regressions.cc \
  -o /tmp/copy-regressions
ASAN_OPTIONS=detect_leaks=0 /tmp/copy-regressions
```

The candidate benchmark build and runner commands are in the parent
[benchmark README](../README.md). Its recorded run is in
[`2026-10-26-copy-candidate`](../results/2026-10-26-copy-candidate/RESULTS.md).

## Counting logical bucket probes

`profile_probes.py` inserts a counter in `probe_bucket`, compiles a temporary
program, and samples 100,000 hits and misses at each size. It uses the exact
integer dataset and shuffles from the benchmark. This instrumentation is **not
used for timing** and does not measure CPU cache misses, branch misses, or
cycles. The published counts are in
[`2026-10-26-probe-profile.csv`](../results/2026-10-26-probe-profile.csv).

| Entries | Occupancy | Hit mean | Hit median | Hit p95 | Hit p99 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1,000,000 | 0.477 | 2.80 | 1 | 8 | 16 |
| 3,000,000 | 0.715 | 4.24 | 2 | 13 | 23 |

These counts explain an opportunity to reduce or overlap scattered metadata
accesses. They do not, by themselves, prove that memory latency dominates CPU
time. The size comparison also changes occupancy; it does not isolate size as
its only independent variable.

The [high-occupancy follow-up](../HIGH_OCCUPANCY.md) benchmarks the promoted
production code. The production regression target now includes copy-count,
partial-rebuild failure, aliasing, and true high-occupancy tests.
