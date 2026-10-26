# Hash-table comparison

Measured 2026-10-26T04:44:10.359596+00:00 on Intel(R) Xeon(R) Platinum 8352V CPU @ 2.10GHz (CPU 0).
compiler: Clang 22.1.8; libstdc++: 20260612. Release build: C++20, `-O3 -DNDEBUG`, without `-march=native`.
Each throughput entry is the median of 5 samples. Lower is better. Values are nanoseconds per map operation.

The runner records all samples in [raw.csv](raw.csv), summaries and quartiles in [summary.csv](summary.csv),
and the build/workload environment in [metadata.json](metadata.json). See [methodology](../../README.md).

## u64, 1,000,000 entries, native hashes

| Table | insert_grow | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std | 951.42 | 137.04 | 156.69 | 152.61 | 43.6 | 0.691 |
| absl | 237.45 | 102.78 | 52.58 | 84.21 | 35.7 | 0.477 |
| dense | 449.81 | 94.03 | 56.65 | 92.12 | 33.6 | 0.477 |
| elastic | 1134.52 | 509.24 | 179.62 | 344.51 | 54.5 | 0.477 |

## u64, 3,000,000 entries, native hashes

| Table | insert_grow | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std | 1205.42 | 139.88 | 157.55 | 151.64 | 47.9 | 0.503 |
| absl | 206.38 | 109.98 | 70.80 | 90.37 | 23.8 | 0.715 |
| dense | 438.88 | 115.74 | 67.24 | 122.92 | 33.6 | 0.715 |
| elastic | 1046.30 | 824.81 | 233.06 | 551.79 | 36.4 | 0.715 |
