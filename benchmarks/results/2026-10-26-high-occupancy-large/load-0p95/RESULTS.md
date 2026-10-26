# Hash-table comparison

Measured 2026-10-26T05:35:40.262954+00:00 on Intel(R) Xeon(R) Platinum 8352V CPU @ 2.10GHz (CPU 0).
compiler: Clang 22.1.8; libstdc++: 20260612. Release build: C++20, `-O3 -DNDEBUG`, without `-march=native`.
Each throughput entry is the median of 5 samples. Lower is better. Values are nanoseconds per map operation.

The runner records all samples in [raw.csv](raw.csv), summaries and quartiles in [summary.csv](summary.csv),
and the build/workload environment in [metadata.json](metadata.json). See [methodology](../../../README.md).

Requested maximum load: 0.95; fixtures preallocated before timing.
Actual occupancies differ by library; see the last column and raw configured_max_load field.

## u64, 996,146 entries, native hashes

| Table | insert_reserved | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std | 277.62 | 92.70 | 162.56 | 100.80 | 40.5 | 0.943 |
| absl | 166.52 | 80.47 | 45.40 | 67.37 | 35.8 | 0.475 |
| dense | 331.75 | 237.28 | 187.82 | 216.70 | 24.4 | 0.950 |
| elastic | 398.09 | 982.84 | 373.94 | 299.85 | 27.4 | 0.950 |
