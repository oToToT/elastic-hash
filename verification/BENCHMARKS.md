# Local benchmark results

Measured on 2026-10-14 with Clang 22.1.8, `-O3 -DNDEBUG`, with both
executables pinned to CPU 0. Each entry is the median of seven samples.
The baseline is the original header before these fixes, compiled as C++23
(it did not compile as advertised under C++20). The replacement uses C++20.
Both executables use the same `benchmark.cc` and produce matching checksums.

| Keys | Operation | Before (ns/op) | After (ns/op) | Before / after |
| ---: | :--- | ---: | ---: | ---: |
| 1024 | insert | 281.7 | 120.8 | 2.33x |
| 1024 | hit | 202.3 | 32.0 | 6.32x |
| 1024 | miss | 194.9 | 13.1 | 14.85x |
| 8192 | insert | 189.2 | 154.4 | 1.23x |
| 8192 | hit | 95.0 | 44.1 | 2.15x |
| 8192 | miss | 244.7 | 14.7 | 16.66x |
| 65536 | insert | 155.5 | 185.6 | 0.84x |
| 65536 | hit | 121.4 | 54.5 | 2.23x |
| 65536 | miss | 330.9 | 16.7 | 19.82x |

These integer-key workloads show faster hits and misses. Insertions include
growth and full duplicate detection: the 65,536-key case is about 19% slower
than the baseline, while the two smaller cases are faster. Fixing uniqueness
requires work the old implementation omitted. The additional fingerprints and
membership filter use approximately two bytes per bucket in total.

Timings in this environment vary considerably. These are local measurements,
not statistical performance guarantees, and they do not establish asymptotic
complexity. No speed claim is made for strings, deletion churn, adversarial
hashes, or high-load configurations from this benchmark.

To reproduce the current implementation:

```sh
clang++ -std=c++20 -O3 -DNDEBUG -I. verification/benchmark.cc -o /tmp/elastic-bench
/tmp/elastic-bench
```

To compare a saved original header with the same workload:

```sh
clang++ -std=c++23 -O3 -DNDEBUG -I. \
  '-DELASTIC_HASH_HEADER="/tmp/elastic_hash_before.hpp"' \
  verification/benchmark.cc -o /tmp/elastic-bench-before
/tmp/elastic-bench-before
```

The Google Benchmark target remains available for broader workloads:

```sh
bazelisk run -c opt //:unordered_map_benchmark
```
