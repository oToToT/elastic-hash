# Hash-table comparison

Measured 2026-10-26T05:33:04.314657+00:00 on Intel(R) Xeon(R) Platinum 8352V CPU @ 2.10GHz (CPU 0).
compiler: Clang 22.1.8; libstdc++: 20260612. Release build: C++20, `-O3 -DNDEBUG`, without `-march=native`.
Each throughput entry is the median of 5 samples. Lower is better. Values are nanoseconds per map operation.

The runner records all samples in [raw.csv](raw.csv), summaries and quartiles in [summary.csv](summary.csv),
and the build/workload environment in [metadata.json](metadata.json). See [methodology](../../../README.md).

Requested maximum load: 0.9; fixtures preallocated before timing.
Actual occupancies differ by library; see the last column and raw configured_max_load field.

## u64, 235,928 entries, native hashes

| Table | insert_reserved | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std | 493.20 | 137.51 | 166.13 | 144.80 | 41.4 | 0.851 |
| absl | 109.97 | 73.55 | 27.77 | 55.04 | 37.8 | 0.450 |
| dense | 193.55 | 122.15 | 52.93 | 92.63 | 24.9 | 0.900 |
| elastic | 338.70 | 509.04 | 112.37 | 363.61 | 28.9 | 0.900 |

## string, 235,928 entries, native hashes

| Table | insert_reserved | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std | 967.71 | 503.83 | 512.12 | 520.39 | 121.4 | 0.851 |
| absl | 554.98 | 340.16 | 89.30 | 245.39 | 139.1 | 0.450 |
| dense | 499.29 | 504.18 | 161.04 | 450.70 | 96.9 | 0.900 |
| elastic | 646.78 | 895.72 | 133.91 | 599.15 | 103.6 | 0.900 |
