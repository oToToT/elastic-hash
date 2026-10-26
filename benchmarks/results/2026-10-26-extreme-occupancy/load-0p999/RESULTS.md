# Hash-table comparison

Measured 2026-10-26T05:37:47.210973+00:00 on Intel(R) Xeon(R) Platinum 8352V CPU @ 2.10GHz (CPU 0).
compiler: Clang 22.1.8; libstdc++: 20260612. Release build: C++20, `-O3 -DNDEBUG`, without `-march=native`.
Each throughput entry is the median of 5 samples. Lower is better. Values are nanoseconds per map operation.

The runner records all samples in [raw.csv](raw.csv), summaries and quartiles in [summary.csv](summary.csv),
and the build/workload environment in [metadata.json](metadata.json). See [methodology](../../../README.md).

Requested maximum load: 0.999; fixtures preallocated before timing.
Actual occupancies differ by library; see the last column and raw configured_max_load field.

## u64, 261,880 entries, native hashes

| Table | insert_reserved | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std | 459.24 | 122.66 | 161.02 | 138.52 | 40.5 | 0.945 |
| absl | 104.40 | 58.18 | 21.31 | 47.44 | 34.0 | 0.499 |
| dense | 1196.54 | 888.42 | 524.44 | 514.04 | 24.0 | 0.999 |
| elastic | 831.60 | 1033.40 | 4813.41 | 2869.63 | 26.0 | 0.999 |

## string, 261,880 entries, native hashes

| Table | insert_reserved | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std | 837.04 | 479.74 | 369.51 | 370.46 | 120.5 | 0.945 |
| absl | 508.48 | 302.30 | 72.03 | 213.97 | 130.1 | 0.499 |
| dense | 1780.42 | 1655.25 | 776.06 | 973.49 | 96.0 | 0.999 |
| elastic | 1136.92 | 1191.89 | 3578.78 | 3392.86 | 98.1 | 0.999 |
