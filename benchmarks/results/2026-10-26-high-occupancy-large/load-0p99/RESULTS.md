# Hash-table comparison

Measured 2026-10-26T05:34:31.702964+00:00 on Intel(R) Xeon(R) Platinum 8352V CPU @ 2.10GHz (CPU 0).
compiler: Clang 22.1.8; libstdc++: 20260612. Release build: C++20, `-O3 -DNDEBUG`, without `-march=native`.
Each throughput entry is the median of 5 samples. Lower is better. Values are nanoseconds per map operation.

The runner records all samples in [raw.csv](raw.csv), summaries and quartiles in [summary.csv](summary.csv),
and the build/workload environment in [metadata.json](metadata.json). See [methodology](../../../README.md).

Requested maximum load: 0.99; fixtures preallocated before timing.
Actual occupancies differ by library; see the last column and raw configured_max_load field.

## u64, 1,038,089 entries, native hashes

| Table | insert_reserved | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std | 467.72 | 138.11 | 183.75 | 156.07 | 40.1 | 0.983 |
| absl | 182.23 | 89.09 | 52.07 | 76.55 | 34.3 | 0.495 |
| dense | 549.78 | 516.17 | 417.73 | 511.51 | 24.1 | 0.990 |
| elastic | 661.57 | 677.85 | 912.28 | 1175.92 | 26.3 | 0.990 |
