# Copy optimization and high occupancy

**The copy reduction is now integrated into `elastic_hash.hpp`. At 99.9%
occupancy, Elastic beats unordered_dense on string hits and on bulk filling,
but misses are much slower. This is a workload-specific crossover, not an
overall win.**

The code change removes an intermediate const-key pair during rvalue insertion
and copies existing entries directly into the replacement table on rebuild.
Observed internal key copies fall from 2 to 1 for both operations, excluding
construction of the caller's pair. It does not change the probe sequence,
insertion-level choices, capacity rules, or scheduler proof.

## Optimization validation

* Both Bazel test targets pass, including the existing million-element tests.
* The standalone regressions pass under AddressSanitizer and UBSan.
* New tests cover aliased duplicate insertion, mapped arguments that alias
  storage across growth, rollback after a partially constructed replacement
  throws, copy-count ceilings, and actual fills up to 99.9% (including constant
  hashes, misses, deletion and reinsertion).
* The dependency-free Lean proofs pass with warnings treated as errors.

An alternating A/B collection used five pairs of processes, each with five
reserved-string-insertion samples, 196,607 entries and 75% occupancy. Binary
order reverses between pairs. The baseline is the pre-optimization header
from commit `75a09af`, compiled using the same updated harness and flags.
The median paired optimized/baseline time ratio was **0.870**, or
**13.0% less insertion time**. Four pairs improved; one was about 2.4%
slower. Variation is substantial, so this is a local measured gain rather than
a guaranteed percentage. Process CPU/wall ratios were about 0.99, ruling out
gross process descheduling for those pairs but not frequency/cache effects.

Separate, noninterleaved before/after blocks varied more and are retained as
raw data; their direct ratios are not used as evidence of a speedup. The A/B
timing excludes reservation and contains no rebuild; the reduction of rebuild
copy counts is tested separately.

## 99.9% occupancy: the observed crossover

261,880 entries; Elastic has 262,143 buckets and unordered_dense has 262,144.
Both therefore achieve approximately 99.9% occupancy. Five-sample medians in
ns/op, lower is better. Build/fill is the mean cost of all insertions from an
empty preallocated table to its final fill; it is not an insertion performed
only after reaching 99.9%.

| Operation | Elastic | unordered_dense | Elastic result |
| :--- | ---: | ---: | :--- |
| Integer build/fill | 831.6 | 1196.5 | 30% less time |
| String build/fill | 1136.9 | 1780.4 | 36% less time |
| String hit | 1191.9 | 1655.2 | 28% less time |
| String miss | 3578.8 | 776.1 | 4.6x slower |
| Integer hit | 1033.4 | 888.4 | 1.2x slower |
| Integer miss | 4813.4 | 524.4 | 9.2x slower |

Elastic used about 98.1 heap B/string key versus 96.0 for unordered_dense,
so these two tables have similar memory footprints in this case. Successful
string-lookup sample ranges did not overlap in this collection (Elastic about
1,051–1,272 ns, unordered_dense about 1,481–1,978 ns). This is still one VM and
one shuffled dataset seed; application-specific validation remains necessary.

Abseil and std remained faster on these lookups, but did not achieve the same
occupancy: Abseil was about 50% full and std about 94.5%. They also used more
heap per string key (about 130 and 120 B respectively). This comparison exposes
the speed/space tradeoff rather than declaring equal-memory competitors.

## Smaller sweep: requested loads 75%, 90%, 95%, 99%, 99.9%

The reference capacity is 262,143 buckets. N is floor(reference * requested
load), identical for all tables at a given requested load. Each fixture sets
max_load_factor(requested) and reserves N entries before timing. Insertion
does not grow the reserved bucket array; the harness verifies that condition.
Key generation, allocation of the initial table, and validation are excluded
from throughput timing. Memory includes the preallocated table and owned keys.

Actual occupancies are shown because std rounds to primes, unordered_dense
rounds to powers of two, and Abseil ignores the max_load_factor setter and
uses its own capacity policy. Abseil's getter returns 1 despite that policy.

### 64-bit integers

| Requested | Table | Entries | Actual fill | Build/fill ns | Hit ns | Miss ns | Heap B/key |
| ---: | :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| 75% | std::unordered_map | 196,607 | 70.91% | 412.6 | 112.6 | 129.7 | 43.3 |
| 75% | absl::flat_hash_map | 196,607 | 75.00% | 81.1 | 24.2 | 22.8 | 22.7 |
| 75% | ankerl::unordered_dense::map | 196,607 | 75.00% | 169.4 | 74.6 | 28.5 | 26.7 |
| 75% | ElasticHashTable | 196,607 | 75.00% | 187.7 | 218.9 | 33.6 | 34.7 |
| 90% | std::unordered_map | 235,928 | 85.09% | 493.2 | 137.5 | 166.1 | 41.4 |
| 90% | absl::flat_hash_map | 235,928 | 45.00% | 110.0 | 73.6 | 27.8 | 37.8 |
| 90% | ankerl::unordered_dense::map | 235,928 | 90.00% | 193.5 | 122.1 | 52.9 | 24.9 |
| 90% | ElasticHashTable | 235,928 | 90.00% | 338.7 | 509.0 | 112.4 | 28.9 |
| 95% | std::unordered_map | 249,035 | 89.82% | 507.3 | 177.3 | 192.0 | 40.9 |
| 95% | absl::flat_hash_map | 249,035 | 47.50% | 97.2 | 61.1 | 23.9 | 35.8 |
| 95% | ankerl::unordered_dense::map | 249,035 | 95.00% | 271.3 | 157.5 | 77.0 | 24.4 |
| 95% | ElasticHashTable | 249,035 | 95.00% | 386.3 | 529.7 | 193.5 | 27.4 |
| 99% | std::unordered_map | 259,521 | 93.60% | 300.8 | 79.0 | 109.0 | 40.6 |
| 99% | absl::flat_hash_map | 259,521 | 49.50% | 66.4 | 56.8 | 33.6 | 34.3 |
| 99% | ankerl::unordered_dense::map | 259,521 | 99.00% | 498.3 | 366.0 | 186.4 | 24.1 |
| 99% | ElasticHashTable | 259,521 | 99.00% | 474.7 | 720.4 | 1010.3 | 26.3 |
| 99.9% | std::unordered_map | 261,880 | 94.45% | 459.2 | 122.7 | 161.0 | 40.5 |
| 99.9% | absl::flat_hash_map | 261,880 | 49.95% | 104.4 | 58.2 | 21.3 | 34.0 |
| 99.9% | ankerl::unordered_dense::map | 261,880 | 99.90% | 1196.5 | 888.4 | 524.4 | 24.0 |
| 99.9% | ElasticHashTable | 261,880 | 99.90% | 831.6 | 1033.4 | 4813.4 | 26.0 |

### 24-character strings

| Requested | Table | Entries | Actual fill | Build/fill ns | Hit ns | Miss ns | Heap B/key |
| ---: | :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| 75% | std::unordered_map | 196,607 | 70.91% | 739.3 | 445.3 | 423.1 | 123.3 |
| 75% | absl::flat_hash_map | 196,607 | 75.00% | 489.1 | 313.3 | 96.3 | 102.7 |
| 75% | ankerl::unordered_dense::map | 196,607 | 75.00% | 428.1 | 331.8 | 120.4 | 98.7 |
| 75% | ElasticHashTable | 196,607 | 75.00% | 714.0 | 689.8 | 89.5 | 114.7 |
| 90% | std::unordered_map | 235,928 | 85.09% | 967.7 | 503.8 | 512.1 | 121.4 |
| 90% | absl::flat_hash_map | 235,928 | 45.00% | 555.0 | 340.2 | 89.3 | 139.1 |
| 90% | ankerl::unordered_dense::map | 235,928 | 90.00% | 499.3 | 504.2 | 161.0 | 96.9 |
| 90% | ElasticHashTable | 235,928 | 90.00% | 646.8 | 895.7 | 133.9 | 103.6 |
| 95% | std::unordered_map | 249,035 | 89.82% | 944.5 | 535.1 | 572.7 | 120.9 |
| 95% | absl::flat_hash_map | 249,035 | 47.50% | 550.6 | 356.9 | 127.1 | 134.3 |
| 95% | ankerl::unordered_dense::map | 249,035 | 95.00% | 498.0 | 527.8 | 144.3 | 96.4 |
| 95% | ElasticHashTable | 249,035 | 95.00% | 942.5 | 1178.2 | 276.0 | 100.6 |
| 99% | std::unordered_map | 259,521 | 93.60% | 816.3 | 356.0 | 424.6 | 120.6 |
| 99% | absl::flat_hash_map | 259,521 | 49.50% | 536.0 | 301.6 | 76.1 | 130.8 |
| 99% | ankerl::unordered_dense::map | 259,521 | 99.00% | 781.1 | 1017.9 | 352.1 | 96.1 |
| 99% | ElasticHashTable | 259,521 | 99.00% | 841.6 | 1247.9 | 1051.4 | 98.5 |
| 99.9% | std::unordered_map | 261,880 | 94.45% | 837.0 | 479.7 | 369.5 | 120.5 |
| 99.9% | absl::flat_hash_map | 261,880 | 49.95% | 508.5 | 302.3 | 72.0 | 130.1 |
| 99.9% | ankerl::unordered_dense::map | 261,880 | 99.90% | 1780.4 | 1655.2 | 776.1 | 96.0 |
| 99.9% | ElasticHashTable | 261,880 | 99.90% | 1136.9 | 1191.9 | 3578.8 | 98.1 |

## Million-slot confirmation

The reference capacity is 1,048,575. These tests use integer keys; they confirm
that 99% does not yet give Elastic a successful-search crossover with dense.
At the 99% point Elastic took about 678 ns/hit versus 516 ns for unordered_dense;
std was about 138 ns at 98.3% actual occupancy, and Abseil 89 ns at 49.5%.

| Requested | Table | Entries | Actual fill | Build/fill ns | Hit ns | Miss ns | Heap B/key |
| ---: | :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| 95% | std::unordered_map | 996,146 | 94.30% | 277.6 | 92.7 | 162.6 | 40.5 |
| 95% | absl::flat_hash_map | 996,146 | 47.50% | 166.5 | 80.5 | 45.4 | 35.8 |
| 95% | ankerl::unordered_dense::map | 996,146 | 95.00% | 331.8 | 237.3 | 187.8 | 24.4 |
| 95% | ElasticHashTable | 996,146 | 95.00% | 398.1 | 982.8 | 373.9 | 27.4 |
| 99% | std::unordered_map | 1,038,089 | 98.27% | 467.7 | 138.1 | 183.8 | 40.1 |
| 99% | absl::flat_hash_map | 1,038,089 | 49.50% | 182.2 | 89.1 | 52.1 | 34.3 |
| 99% | ankerl::unordered_dense::map | 1,038,089 | 99.00% | 549.8 | 516.2 | 417.7 | 24.1 |
| 99% | ElasticHashTable | 1,038,089 | 99.00% | 661.6 | 677.8 | 912.3 | 26.3 |

## Interpretation

Elastic can be competitive for very densely packed, mostly successful string
lookups or bulk construction. Its current missing-key path degrades sharply
near full occupancy. A false positive from the global membership filter can
lead to a long search across almost-full or full levels. The logical search
is bounded and correct, but costly. The paper's successful-search guarantee
does not establish a bound for this engineering negative-query extension.

No occupancy from 75% through 99% produced a consistent successful-lookup
win over dense in these data. The 99.9% win should not be extrapolated to
ordinary load factors, miss-heavy workloads, or every key type. Std is a
node-based chained table, and the practical flat maps have different metadata
and relocation policies from the paper's fixed-array/no-reordering model.
Slot-probe bounds alone are not a universal wall-clock speed ranking.

## Reproduce and inspect

Measured on the same Intel Xeon Platinum 8352V VM, CPU 0, Clang 22.1.8, C++20,
`-O3 -DNDEBUG`, libstdc++ build 20260612 and glibc 2.43 as the earlier studies.
Compilation and validation finished before timing. All timing collections ran
serially with CPU affinity fixed. Actual hashes of each header, executable,
and harness plus the precise commands are recorded with every run.

* [75–99% sweep](results/2026-10-26-high-occupancy/campaign.json)
* [Million-slot 95–99% sweep](results/2026-10-26-high-occupancy-large/campaign.json)
* [99.9% extreme case](results/2026-10-26-extreme-occupancy/load-0p999/RESULTS.md)
* [Alternating optimization A/B samples](results/2026-10-26-copy-paired/pairs.csv)
* [Noninterleaved historical-baseline blocks](results/2026-10-26-high-occupancy-before-copy/campaign.json)
* [Build and runner instructions](README.md)

Each load directory contains full raw samples, median/quartile/range summaries,
mixed-query timings, heap estimates, and metadata. To repeat the sweep, use
`run_occupancy.py --buckets 262143 --loads 0.75 0.90 0.95 0.99 0.999`.
The historical pre-copy header can be reconstructed with
`git show 75a09af:elastic_hash.hpp`; the optional alternate-header target builds
it with the same harness. `experiments/compare_copy.py` collects alternating
A/B runs. The original scheduler proofs remain partial: deterministic hashing,
batch probability estimates, deletions, and full C++ refinement are not proved.
