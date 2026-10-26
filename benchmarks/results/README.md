# Measurement archive

These are observations from one Intel Xeon Platinum 8352V VM on 2026-10-26,
with Clang 22.1.8, C++20 release builds and libstdc++ build 20260612. The
[methodology](../README.md) defines the workloads, hash modes, sampling and
heap estimates. The reports retain negative results and speed/space tradeoffs.

| Collection | Header and purpose | Interpretation |
| :--- | :--- | :--- |
| [Default-load comparison](2026-10-26-clang22/RESULTS.md) | Pre-copy header; six tables, three sizes, integers/strings and native/shared hashers | Historical baseline; optimized flat tables generally won hits and insertion. |
| [Large integers](2026-10-26-large-u64/RESULTS.md) | Pre-copy header; one and three million entries | Size alone did not produce a successful-lookup crossover. |
| [Large strings](2026-10-26-large-string/RESULTS.md) | Pre-copy header; one million entries | Elastic misses beat std, while successful searches lost. |
| [Copy candidate](2026-10-26-copy-candidate/RESULTS.md) | Experimental copy reduction, since integrated | Noninterleaved timings are preliminary; use the paired collection for a controlled before/after comparison. |
| [Logical probes](2026-10-26-probe-profile.csv) | Instrumented integer lookups on the benchmark dataset | Bucket-probe counts, not hardware cache misses or timings. |
| [75–99% occupancy](2026-10-26-high-occupancy/campaign.json) | Integrated copy reduction; integer and string keys | Actual occupancy and memory differ by library. |
| [Million-slot occupancy](2026-10-26-high-occupancy-large/campaign.json) | Integrated copy reduction; integers, 95%/99% requested | No successful-lookup crossover with unordered_dense at 99%. |
| [99.9% occupancy](2026-10-26-extreme-occupancy/load-0p999/RESULTS.md) | Integrated copy reduction; about 262,000 entries | String-hit and bulk-fill wins over similarly filled unordered_dense, with much slower misses. |
| [Historical-header blocks](2026-10-26-high-occupancy-before-copy/campaign.json) | Pre-copy header compiled with the occupancy harness | Separate blocks varied substantially; their direct timing ratios are not used as evidence of a speedup. |
| [Alternating A/B](2026-10-26-copy-paired/pairs.csv) | Pre-copy versus integrated copy reduction; five alternating pairs | Median paired reserved-string-insertion time fell 13%; results varied between pairs. |

## Files and provenance

* `raw.csv` preserves individual sample values, checksums, bucket counts and
  other recorded fields. CSV line endings are normalized to LF for Git; cell
  contents are unchanged.
* `summary.csv` contains medians, selected sample quartiles, ranges and ratios
  to std when that reference is present. Lower time ratios are better.
* `metadata.json` records the environment and commands plus header, harness and
  executable SHA-256 hashes. Fields were added as the harness evolved.
* `campaign.json` lists the completed requested-load points. Each point has its
  own raw data, summaries and metadata.
* The paired collection records per-process CPU/wall times and pair medians.

Recorded commands include execution-time checkout and `/tmp` paths. They are
provenance, not portable paths to reuse in a fresh checkout. Build fresh
executables with the [repository-relative instructions](../README.md).
Historical headers can be recreated from `git show 75a09af:elastic_hash.hpp`;
the integrated copy change is also retained in the
[experiment patch](../experiments/avoid_key_copies.patch).

The recorded base Git commit alone does not identify the full measured working
tree: benchmark tooling and, for later runs, the copy change were uncommitted
when measured. The content hashes distinguish those versions. The harness
subsequently gained operation selection and occupancy support, so older harness
hashes can differ from the current file. Historical executable binaries and every
intermediate harness revision are not archived; use the current tools to repeat
the documented workloads, rather than promise byte-identical historical builds.

Do not combine these tables as if they were one confidence interval or an
identical memory-budget comparison. Samples use one shuffled seed and warmed
queries. Growth thresholds change actual occupancy; reservation costs and
final destruction are excluded from insertion timing. Heap estimates include
map-owned strings and allocator rounding, and are not RSS or peak memory.
