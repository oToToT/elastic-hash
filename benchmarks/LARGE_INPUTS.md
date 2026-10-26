# Larger inputs and optimization opportunities

**Historical measurements:** the copy reduction has since been promoted to
production. See the [high-occupancy follow-up](HIGH_OCCUPANCY.md) for current results.

**Larger input did not produce a successful-lookup crossover in these tests.**
At one and three million integer keys, Elastic remained slower than std,
Abseil, and unordered_dense. At one million string keys it also lost successful
lookups, while its misses were faster than std. At three million integers it
did beat std on insertion with growth; both optimized flat maps still won.

These runs use the same VM, compiler, key generation, native hashes, and
default load policies as the [earlier comparison](COMPARISON.md), with five
samples per operation. Compilation and probe instrumentation finished before
timing collection. Large runs select growth insertion and hit/miss/mixed
lookups; they do not repeat the earlier expensive erase/churn suite.
Times below are medians in ns/op. Memory is approximate incremental glibc
heap bytes per entry. Every fixture and pass validates its result.

## 1,000,000 integer keys

| Table | Insert/grow | Hit | Miss | 50% hit | Heap B/key | Occupancy |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std::unordered_map | 951.4 | 137.0 | 156.7 | 152.6 | 43.6 | 0.691 |
| absl::flat_hash_map | 237.5 | 102.8 | 52.6 | 84.2 | 35.7 | 0.477 |
| ankerl::unordered_dense::map | 449.8 | 94.0 | 56.6 | 92.1 | 33.6 | 0.477 |
| ElasticHashTable | 1134.5 | 509.2 | 179.6 | 344.5 | 54.5 | 0.477 |

## 3,000,000 integer keys

| Table | Insert/grow | Hit | Miss | 50% hit | Heap B/key | Occupancy |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std::unordered_map | 1205.4 | 139.9 | 157.5 | 151.6 | 47.9 | 0.503 |
| absl::flat_hash_map | 206.4 | 110.0 | 70.8 | 90.4 | 23.8 | 0.715 |
| ankerl::unordered_dense::map | 438.9 | 115.7 | 67.2 | 122.9 | 33.6 | 0.715 |
| ElasticHashTable | 1046.3 | 824.8 | 233.1 | 551.8 | 36.4 | 0.715 |

## 1,000,000 string keys (24 characters)

| Table | Insert/grow | Hit | Miss | 50% hit | Heap B/key | Occupancy |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std::unordered_map | 1472.8 | 603.9 | 535.7 | 594.0 | 123.6 | 0.691 |
| absl::flat_hash_map | 973.0 | 404.5 | 164.9 | 329.4 | 134.0 | 0.477 |
| ankerl::unordered_dense::map | 696.4 | 527.9 | 185.8 | 516.6 | 106.7 | 0.477 |
| ElasticHashTable | 3606.9 | 1048.1 | 211.8 | 644.9 | 152.9 | 0.477 |

Actual occupancy changes around growth thresholds. These sizes do not hold
occupancy constant, and the maps do not have equal memory budgets. The results
therefore describe real default-growth behavior rather than isolate input size
as the sole variable.

## Why growing N is not the theoretical advantage

The [paper](https://arxiv.org/html/2501.02305) studies the dependence on the
free-space fraction delta. With a fixed maximum load, making N larger does not
create an asymptotic advantage over other expected-constant-time tables.
These default-load runs did not test the near-full regime; the
[follow-up](HIGH_OCCUPANCY.md) measures requested loads through 99.9%. Our deterministic mixer,
deletions, and engineering extensions also remain outside a complete proof of
the paper's probabilistic guarantees.

The working sets here exceed cache. On the exact integer dataset, a separate
instrumented run sampled 100,000 queries and counted successful bucket probes:

| Entries | Elastic occupancy | Mean | Median | p95 | p99 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1,000,000 | 0.477 | 2.80 | 1 | 8 | 16 |
| 3,000,000 | 0.715 | 4.24 | 2 | 13 | 23 |

These are logical bucket-probe counts, not hardware cache-miss counts. They
support investigating scattered accesses and scheduler work, but do not prove
which CPU resource dominates runtime. Occupancy also differs between these rows.

## Historical copy-reduction candidate (now integrated)

The small [patch](experiments/avoid_key_copies.patch) removes an intermediate
const-key pair on insertion and forwards existing entries directly into a
replacement table on rebuild. The measured internal key-copy counts fell from
**2 to 1** per rvalue insertion and per rebuilt entry. This excludes construction
of the caller's input pair.

For one million string keys, insertion with growth changed from **3607 to
3249 ns/key**, or **9.9% less time** in this collection. This is
promising but preliminary: the five-sample ranges overlap, and there was no
interleaved repeated A/B collection. The lookup algorithm is unchanged; the
candidate does not close the successful-search gap. Its reserved-insertion
timings are recorded separately, without a matched million-entry baseline
reserved run.

The candidate passed the full standalone regression suite under ASan/UBSan,
plus aliased duplicate insertion, aliased try_emplace arguments during growth,
and failure during rebuild. It changes no probing, placement choices, load
policies, or scheduler bound. See the [experiment notes](experiments/README.md)
for commands, safety reasoning, and the observed copy counts.

## Further opportunities, in practical order

1. **Avoid copying keys and values unnecessarily.** The tested patch is a small
   first step. Constructing try_emplace entries directly in their destination
   slot could remove more copies, with careful handling of arguments that alias
   table storage across growth.
2. **Shrink slots.** Occupancy is represented both by optional values and by
   metadata bytes. A storage layer using metadata to manage object lifetimes
   could reduce padding and memory traffic. It needs careful constructor,
   destructor, move, and exception-safety testing.
3. **Overlap independent probes.** Precomputing a small window of probe addresses
   and prefetching metadata could hide latency while checking candidates in the
   same logical order. It needs benchmark validation; eager loads can waste
   bandwidth and make short searches slower.
4. **Make filter and insertion bookkeeping cheaper.** A one-cache-line blocked
   membership filter, cached probe limits, and less expensive tombstone cleanup
   are candidates. Filter changes can affect false-positive rates; no speedup
   for these ideas is claimed here.
5. **Consider grouped probing as an architectural experiment.** Abseil compares
   multiple consecutive metadata bytes with SIMD, as its
   [design notes](https://abseil.io/about/design/swisstables) explain. Adopting
   cache-line groups in an elastic level could offer a larger gain, but changes
   the independent-slot probe model and would need its own correctness and
   complexity analysis. The existing scheduler proof alone would not suffice.

## Recorded data and reproduction

* [Large integer run](results/2026-10-26-large-u64/RESULTS.md)
* [Large string run](results/2026-10-26-large-string/RESULTS.md)
* [Copy candidate run](results/2026-10-26-copy-candidate/RESULTS.md)
* [Logical probe counts](results/2026-10-26-probe-profile.csv)
* [Runner/build instructions](README.md)

Each run directory includes all raw samples, median/range summaries, source and
binary hashes, and environment metadata. Treat small differences as tentative
on this VM; the large successful-lookup gap is the more consistent observation.
