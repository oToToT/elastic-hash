# Hash-table comparison

Measured 2026-10-26T04:47:43.983857+00:00 on Intel(R) Xeon(R) Platinum 8352V CPU @ 2.10GHz (CPU 0).
compiler: Clang 22.1.8; libstdc++: 20260612. Release build: C++20, `-O3 -DNDEBUG`, without `-march=native`.
Each throughput entry is the median of 5 samples. Lower is better. Values are nanoseconds per map operation.

The runner records all samples in [raw.csv](raw.csv), summaries and quartiles in [summary.csv](summary.csv),
and the build/workload environment in [metadata.json](metadata.json). See [methodology](../../README.md).

## string, 1,000,000 entries, native hashes

| Table | insert_grow | hit | miss | mixed | Heap B/key | Load |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| std | 1472.78 | 603.95 | 535.65 | 593.95 | 123.6 | 0.691 |
| absl | 973.03 | 404.54 | 164.85 | 329.44 | 134.0 | 0.477 |
| dense | 696.38 | 527.94 | 185.81 | 516.56 | 106.7 | 0.477 |
| elastic | 3606.93 | 1048.11 | 211.78 | 644.88 | 152.9 | 0.477 |
