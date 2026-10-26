# Hash-table comparison

Measured 2026-10-26T05:33:32.404413+00:00 on Intel(R) Xeon(R) Platinum 8352V CPU @ 2.10GHz (CPU 0).
compiler: Clang 22.1.8; libstdc++: 20260612. Release build: C++20, `-O3 -DNDEBUG`, without `-march=native`.
Each throughput entry is the median of 5 samples. Lower is better. Values are nanoseconds per map operation.

The runner records all samples in [raw.csv](raw.csv), summaries and quartiles in [summary.csv](summary.csv),
and the build/workload environment in [metadata.json](metadata.json). See [methodology](../../../README.md).

Requested maximum load: 0.99; fixtures preallocated before timing.
Actual occupancies differ by library; see the last column and raw configured_max_load field.

## u64, 259,521 entries, native hashes

| Table | insert_reserved | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std | 300.83 | 79.03 | 108.99 | 85.60 | 40.6 | 0.936 |
| absl | 66.36 | 56.84 | 33.59 | 64.86 | 34.3 | 0.495 |
| dense | 498.27 | 366.02 | 186.36 | 281.39 | 24.1 | 0.990 |
| elastic | 474.74 | 720.44 | 1010.27 | 582.48 | 26.3 | 0.990 |

## string, 259,521 entries, native hashes

| Table | insert_reserved | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std | 816.28 | 355.96 | 424.62 | 427.40 | 120.6 | 0.936 |
| absl | 536.00 | 301.63 | 76.07 | 232.19 | 130.8 | 0.495 |
| dense | 781.14 | 1017.92 | 352.07 | 787.48 | 96.1 | 0.990 |
| elastic | 841.62 | 1247.86 | 1051.38 | 1076.81 | 98.5 | 0.990 |
