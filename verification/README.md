# Verification and complexity contract

Run `./verification/check.sh` from a checkout with a C++20 compiler and Lean
4.24.0 (the checked version). The proof uses only Lean's bundled `Std`, with no
Mathlib download. The script treats proof warnings as errors and then runs the
standalone C++ regressions. The original GoogleTest suite is also available via
`bazelisk test -c opt //:elastic_hash_test //:elastic_hash_regression_test`.

## What the proof establishes

`Scheduler.lean` proves facts about the exact integer allowance used by
`elastic_hash_detail::epoch_probe_limit`:

* A level numbered `i >= 1` belongs to band `k = floor(log2(i))`.
* In epoch `r`, its cumulative allowance is `2^floor((r-k)/2)` when `k <= r`.
* Each band `k` contains `2^k` levels. `bandTotal` explicitly sums those
  allowances. `epoch_work_counts_bands` proves that this sum equals
  `epochWork`, whose recurrence is `W(r+2) = 2 W(r) + 3*2^(r+1)`.
* `epoch_work_bound` proves `W(r) <= 4*2^r` for every natural `r`.
* `target_is_scheduled` proves that every positive pair `(i,j)` is eligible
  by `r = floor(log2(i)) + 2*(floor(log2(j))+1)`.
* `target_rank_bound` proves `W(r) <= 16*i*j*j`. Thus the actual rank of
  the target probe is at most `16*i*j²`. Skipping exhausted/empty levels,
  stopping early, and applying the membership filter can only reduce probes.
* `fallback_injective` proves that the fallback cycle has no repeated slot
  for its first `capacity` steps, for any hash and positive capacity.
* `weighted_cost_bound` and `conditional_complexity_transfer` prove the
  finite weighted-sum transfer: a bound on the weighted moment `i*j²`
  implies the corresponding cost bound with a factor of at most 16.

These are unbounded integer proofs checked by Lean's kernel, not timing tests
or enumeration up to a chosen size. Their printed axiom dependencies are only
Lean's standard `propext` and `Quot.sound`; there are no `sorry` placeholders or
assumed complexity theorems. The C++ regressions additionally compare actual
scheduler allowances with the proved recurrence and check emitted ranks.

## What is not proved

This is **not an end-to-end formal verification of the C++ container or of
Theorem 1** in [the paper](https://arxiv.org/html/2501.02305). In particular:

* The paper analyzes insertion-only operation with independent random probes.
  The C++ mixer is deterministic; an arbitrary `Hash` can even return a constant.
* The probability estimates for elastic batches, their moment bounds, and a
  sufficient value of the constant `C` are not formalized here. The implementation
  retains `C=200` as a tuning choice, not as a proved sufficient constant.
* The conditional transfer theorem **takes** a moment bound as a hypothesis;
  it does not establish that hypothesis for this implementation. Its finite
  weighted model does not formalize general infinite probability spaces.
* Deletion, tombstone cleanup, resizes, negative searches, duplicate checking,
  the membership filter, floating-point batch thresholds, and C++ memory safety
  are covered by implementation reasoning and tests, not a refinement proof.
* The practical probe stream changes from mixed probes to a linear fallback
  after `capacity` attempts. The rank bound still applies, but the paper's
  independent-probe distribution does not apply to that fallback.

Therefore this repository does **not** promise the paper's expected bounds for
all public operations. In particular, checking for duplicates entails a negative
lookup, and rebuilding can move every element. A deterministic constant-hash
counterexample precludes claiming the paper's bounds for every permitted `Hash`.

## Deterministic safeguards in the implementation

Each level's probe stream consists of `capacity` mixed probes followed by a full
linear cycle of `capacity` probes. Insertions therefore find a free slot within
`2*capacity` attempts whenever the batch has space. `max_probe` records the
largest probe index used in that level. Lookup never searches beyond this bound,
so all levels together take at most `2*bucket_count()` slot probes, even on a
miss, with full levels, or with a constant hash. This is a deterministic upper
bound on **slot probes**, not the paper's expected bound or a statement about
arbitrary user-supplied hash/equality execution time.

Tombstones preserve the search path. Live counts determine which levels can be
skipped; historical occupancy determines insertion batches. Cleanup rebuilds
both counts. The filter sets two bits on insertion, never clears them on erase,
and is reconstructed on rebuild, so its negative answers cannot hide live keys.

The dyadic scheduler avoids squaring probe indices and never shifts by the word
width. Its allowances saturate at the recorded limit. Once all limits are
reached, the active mask becomes zero; energy overflow cannot cause a loop.

## Tests and measurements

`elastic_hash_regression_test.cc` includes randomized comparisons against
`std::unordered_map`, loads from 0.01 to 1.0, constant hashes, small/full levels,
duplicate keys across batches, deletion/reinsertion, const and mixed iterators,
copy/move operations, move-only values, stateful hashes, reserve guarantees,
invalid settings, and capacity overflow.

For AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
clang++ -std=c++20 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -I. elastic_hash_regression_test.cc -o /tmp/elastic-regressions
/tmp/elastic-regressions
```

If the runtime is under a tracer that prevents LeakSanitizer from operating,
use `ASAN_OPTIONS=detect_leaks=0` for that run. Address and undefined-behavior
checks remain enabled.

`benchmark.cc` reports the median of seven samples and prints checksums to keep
lookups observable. It measures fresh-table insertion (including growth), hits,
and misses with integer keys. The existing Google Benchmark target covers more
types and operations. See `BENCHMARKS.md` for measured results and commands.
