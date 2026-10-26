# Cross-container results

**Historical measurements:** the copy reduction has since been promoted to
production. See the [high-occupancy follow-up](HIGH_OCCUPANCY.md) for current results.

[Follow-up: million-entry runs and optimization experiments](LARGE_INPUTS.md).

**The measured pre-copy-optimization version was generally slower than the optimized flat maps.**
Successful lookup was slower than std, Abseil, and unordered_dense in every one
of the 12 key-type/size/hash-mode configurations. Abseil and unordered_dense
also beat Elastic on insertion with growth in all 12 configurations.

Elastic did better on missing keys: it beat std in 9 of 12 configurations,
Abseil in 2, and unordered_dense in 3. Results depend strongly on workload.
Iteration is much better than std, while erase/reinsert churn is expensive.

Measured on an Intel Xeon Platinum 8352V VM, CPU 0, Clang 22.1.8, C++20,
`-O3 -DNDEBUG`, libstdc++ build 20260612, glibc 2.43. The complete run covers
six tables, three sizes (4,096 / 45,000 / 180,000), two key types, and native
and shared hashes: **72 configurations, seven samples per operation, 4,104 raw
rows including memory measurements**. Every fixture checked stored key/value
pairs, and every timed pass checked its result. No compilation ran during the
published collection.

The tables use their default load policies; their memory budgets differ.
Reported heap estimates and loads describe the grown lookup fixture. Reserved
insertion excludes reservation time and may have a different load, recorded
separately in the raw CSV. Lower times are better.

## 45,000 64-bit integer keys, native hashes

| Table | Insert/grow ns | Hit ns | Miss ns | Churn ns/op | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std::unordered_map | 247.2 | 32.0 | 44.7 | 160.1 | 47.3 | 0.528 |
| absl::flat_hash_map | 105.2 | 14.7 | 13.1 | 69.4 | 24.9 | 0.687 |
| ankerl::unordered_dense::map | 165.2 | 39.1 | 16.9 | 160.0 | 35.2 | 0.687 |
| GNU gp_hash_table | 235.0 | 15.1 | 27.5 | 63.3 | 70.0 | 0.343 |
| GNU cc_hash_table | 165.9 | 14.8 | 13.5 | 78.9 | 55.5 | 0.343 |
| ElasticHashTable | 407.7 | 242.1 | 45.7 | 1081.2 | 38.0 | 0.687 |

## 45,000 24-character string keys, native hashes

| Table | Insert/grow ns | Hit ns | Miss ns | Churn ns/op | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std::unordered_map | 902.7 | 382.9 | 298.8 | 807.8 | 127.3 | 0.528 |
| absl::flat_hash_map | 438.1 | 212.2 | 68.3 | 319.4 | 107.8 | 0.687 |
| ankerl::unordered_dense::map | 268.7 | 326.7 | 57.5 | 433.1 | 118.1 | 0.687 |
| GNU gp_hash_table | 1133.0 | 251.9 | 199.0 | 472.9 | 188.0 | 0.343 |
| GNU cc_hash_table | 671.2 | 309.7 | 140.4 | 522.9 | 135.5 | 0.343 |
| ElasticHashTable | 1491.3 | 546.8 | 68.4 | 4140.8 | 121.0 | 0.687 |

GNU PBDS tables use about half the lookup occupancy of the flat tables here;
their extra buckets contribute to their lookup speed and memory consumption.
For 45,000 integer keys, Elastic uses about 38 B/key versus 47 for std, 35 for
unordered_dense, and 25 for Abseil. Heap figures are glibc allocation estimates,
including owned key allocations and rounding; they are not RSS or peak memory.

At 45,000 native string keys, Elastic misses take about 68 ns versus 299 ns for
std, essentially tie Abseil at 68 ns, and trail unordered_dense at 58 ns.
Successful string lookups and churn remain slower than those competitors.

The shared-hash runs use the same caller-supplied unordered_dense hasher.
They support the same broad conclusion: Elastic loses every successful lookup
and grow-insertion comparison to Abseil and unordered_dense. Library-specific
mixing and hash caching still vary. See the full tables for exceptions on misses.

Seven samples expose some variability rather than establish a confidence
interval. The largest selected interquartile ratio in the run is about 1.68;
small differences and near-ties should not be treated as a stable ranking.
These results cover one VM, one shuffled dataset seed, warmed query streams,
and the pinned library versions below; they do not establish universal speed
or asymptotic complexity.

* [Complete tables](results/2026-10-26-clang22/RESULTS.md)
* [Raw samples](results/2026-10-26-clang22/raw.csv)
* [Medians, quartiles, and ratios](results/2026-10-26-clang22/summary.csv)
* [Environment, versions, source hashes, and command](results/2026-10-26-clang22/metadata.json)
* [Methodology and reproduction instructions](README.md)
