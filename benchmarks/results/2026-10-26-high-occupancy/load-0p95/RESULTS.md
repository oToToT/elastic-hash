# Hash-table comparison

Measured 2026-10-26T05:32:31.379846+00:00 on Intel(R) Xeon(R) Platinum 8352V CPU @ 2.10GHz (CPU 0).
compiler: Clang 22.1.8; libstdc++: 20260612. Release build: C++20, `-O3 -DNDEBUG`, without `-march=native`.
Each throughput entry is the median of 5 samples. Lower is better. Values are nanoseconds per map operation.

The runner records all samples in [raw.csv](raw.csv), summaries and quartiles in [summary.csv](summary.csv),
and the build/workload environment in [metadata.json](metadata.json). See [methodology](../../../README.md).

Requested maximum load: 0.95; fixtures preallocated before timing.
Actual occupancies differ by library; see the last column and raw configured_max_load field.

## u64, 249,035 entries, native hashes

| Table | insert_reserved | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std | 507.35 | 177.29 | 192.04 | 179.36 | 40.9 | 0.898 |
| absl | 97.20 | 61.14 | 23.91 | 48.05 | 35.8 | 0.475 |
| dense | 271.28 | 157.53 | 77.04 | 191.57 | 24.4 | 0.950 |
| elastic | 386.26 | 529.72 | 193.51 | 406.00 | 27.4 | 0.950 |

## string, 249,035 entries, native hashes

| Table | insert_reserved | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std | 944.52 | 535.07 | 572.70 | 566.50 | 120.9 | 0.898 |
| absl | 550.64 | 356.89 | 127.14 | 265.23 | 134.3 | 0.475 |
| dense | 498.05 | 527.76 | 144.30 | 302.81 | 96.4 | 0.950 |
| elastic | 942.48 | 1178.22 | 276.04 | 720.74 | 100.6 | 0.950 |
