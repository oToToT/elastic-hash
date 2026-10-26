# Hash-table comparison

Measured 2026-10-26T05:36:26.183759+00:00 on Intel(R) Xeon(R) Platinum 8352V CPU @ 2.10GHz (CPU 0).
compiler: Clang 22.1.8; libstdc++: 20260612. Release build: C++20, `-O3 -DNDEBUG`, without `-march=native`.
Each throughput entry is the median of 5 samples. Lower is better. Values are nanoseconds per map operation.

The runner records all samples in [raw.csv](raw.csv), summaries and quartiles in [summary.csv](summary.csv),
and the build/workload environment in [metadata.json](metadata.json). See [methodology](../../../README.md).

Requested maximum load: 0.75; fixtures preallocated before timing.
Actual occupancies differ by library; see the last column and raw configured_max_load field.

## string, 196,607 entries, native hashes

| Table | insert_reserved | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| elastic | 433.14 | 394.13 | 47.38 | 253.55 | 114.7 | 0.750 |
