# Hash-table comparison

Measured 2026-10-26T04:49:53.768600+00:00 on Intel(R) Xeon(R) Platinum 8352V CPU @ 2.10GHz (CPU 0).
compiler: Clang 22.1.8; libstdc++: 20260612. Release build: C++20, `-O3 -DNDEBUG`, without `-march=native`.
Each throughput entry is the median of 5 samples. Lower is better. Values are nanoseconds per map operation.

The runner records all samples in [raw.csv](raw.csv), summaries and quartiles in [summary.csv](summary.csv),
and the build/workload environment in [metadata.json](metadata.json). See [methodology](../../README.md).

## string, 45,000 entries, native hashes

| Table | insert_grow | insert_reserved | hit | miss | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| elastic | 1408.03 | 661.94 | 650.20 | 71.92 | 121.0 | 0.687 |

## string, 1,000,000 entries, native hashes

| Table | insert_grow | insert_reserved | hit | miss | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| elastic | 3248.60 | 1226.18 | 1056.62 | 232.47 | 152.9 | 0.477 |
