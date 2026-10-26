# Hash-table comparison

Measured 2026-10-26T05:34:12.499967+00:00 on Intel(R) Xeon(R) Platinum 8352V CPU @ 2.10GHz (CPU 0).
compiler: Clang 22.1.8; libstdc++: 20260612. Release build: C++20, `-O3 -DNDEBUG`, without `-march=native`.
Each throughput entry is the median of 5 samples. Lower is better. Values are nanoseconds per map operation.

The runner records all samples in [raw.csv](raw.csv), summaries and quartiles in [summary.csv](summary.csv),
and the build/workload environment in [metadata.json](metadata.json). See [methodology](../../../README.md).

Requested maximum load: 0.75; fixtures preallocated before timing.
Actual occupancies differ by library; see the last column and raw configured_max_load field.

## u64, 196,607 entries, native hashes

| Table | insert_reserved | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std | 412.64 | 112.64 | 129.72 | 134.13 | 43.3 | 0.709 |
| absl | 81.14 | 24.22 | 22.80 | 33.92 | 22.7 | 0.750 |
| dense | 169.40 | 74.56 | 28.48 | 93.77 | 26.7 | 0.750 |
| elastic | 187.68 | 218.94 | 33.57 | 156.73 | 34.7 | 0.750 |

## string, 196,607 entries, native hashes

| Table | insert_reserved | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std | 739.34 | 445.32 | 423.09 | 471.96 | 123.3 | 0.709 |
| absl | 489.12 | 313.34 | 96.35 | 236.39 | 102.7 | 0.750 |
| dense | 428.09 | 331.84 | 120.38 | 342.42 | 98.7 | 0.750 |
| elastic | 714.03 | 689.82 | 89.46 | 459.50 | 114.7 | 0.750 |
