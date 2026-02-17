# Elastic Hashing Implementation

This is a C++20 implementation of **Elastic Hashing**, an open-addressing hash table algorithm that achieves optimal theoretical bounds:
*   $O(1)$ amortized expected probe complexity.
*   $O(\log \delta^{-1})$ worst-case expected probe complexity, where $\delta$ is the fraction of empty slots.

Based on the paper: [Optimal Bounds for Open Addressing Without Reordering (arXiv:2501.02305)](https://arxiv.org/html/2501.02305).

## Overview of the Algorithm

Elastic Hashing works by partitioning the hash table into multiple exponentially decreasing "levels" or "sub-tables" ($A_1, A_2, \dots, A_k$). Insertions are processed in batches.

*   **Structure**: The table is split into disjoint arrays $A_i$ where $|A_{i+1}| \approx |A_i|/2$.
*   **Insertion Logic**: Items are inserted into levels based on current load factors. If a level is too full, items "spill over" to the next level. This ensures that even as the table fills up (high load factor), most items reside in levels where they can be found quickly.
*   **Probe Sequence**: Each item has a deterministic probe sequence within each level. The algorithm guarantees that we find an empty slot (for insertion) or our target item (for lookup) with high probability within a small number of probes.

## Key Implementation Differences & Optimizations

While this implementation faithfully follows the core logic of the paper, we introduced a critical optimization for modern hardware: **Energy-Based Probing**.

### 1. Energy-Based Probing (Cache-Friendly Traversal)

The original paper defines a specific bijection $\phi(i, j)$ (mapping probe rank to level $i$ and index $j$) using bit-interleaving to prove the $O(i \cdot j^2)$ cost bound. However, this order jumps between levels erratically, causing severe cache thrashing on real hardware.

**Our Optimization**:
We replaced the bit-interleaving $\phi$ with an **Energy-Based** traversal order.
*   We define the "energy" or "cost" of a probe at level $i$, index $j$ as $E(i, j) = i \cdot j^2$.
*   We visit probes in increasing order of $E(i, j)$.
*   This allows us to **batch** probes by level. In each "energy epoch" $T$, we scan a contiguous range of indices $j$ for each active level $i$ such that $i \cdot j^2 \le T$.

**Benefits**:
*   **Spatial Locality**: We access the same memory region (level $A_i$) multiple times consecutively before switching to another level.
*   **Reduced Overhead**: Eliminates complex bit-manipulation instructions for `inverse_phi`.
*   **Correctness**: We have proven that this ordering strictly satisfies the $O(i \cdot j^2)$ rank requirement. (See **Appendix: Proof of Correctness** below).

This change yields a **20-40% speedup** in lookups compared to the strict paper implementation.

### 2. RapidHash
We use [rapidhash](https://github.com/Nicoshev/rapidhash), a very fast and high-quality non-cryptographic hash function, to minimize hashing overhead.

### 3. Simplified Constants
We use slightly tuned constants ($C=200$, first round load factor $= 0.75$) based on empirical testing, which aligns with the paper's flexibility on these parameters.

## Usage

This is a header-only library. Include `elastic_hash.hpp` in your project.

### Basic Example

```cpp
#include "elastic_hash.hpp"
#include <iostream>
#include <string>

int main() {
    // ElasticHashTable<Key, Value>
    ElasticHashTable<std::string, int> map;

    // Insertion
    map.insert({"apple", 1});
    map["banana"] = 2; // operator[] works as expected

    // Lookup
    if (map.contains("apple")) {
        std::cout << "Apple value: " << map["apple"] << std::endl;
    }

    // Iterator
    for (const auto& [key, value] : map) {
        std::cout << key << ": " << value << std::endl;
    }
    
    return 0;
}
```

### Building & Testing

The project uses **Bazel**.

**Run benchmark**:
```bash
bazelisk run -c opt //:unordered_map_benchmark
```
*Compares `ElasticHashTable` against `std::unordered_map` and `__gnu_pbds` hash tables.*

**Run tests**:
```bash
bazelisk test -c opt //:elastic_hash_test
```

## Performance

Elastic Hashing provides **optimal theoretical bounds** ($O(1)$ amortized expected, $O(\log \delta^{-1})$ worst-case expected probe complexity) which guarantees robust performance even under adversarial conditions where standard hash tables might degenerate.

Nonetheless, when comapring with highly optimized standard libraries (like `std::unordered_map` or `__gnu_pbds::cc_hash_table`), this elastic hash impelementatino still falls behind them. This project is currently just a starting point for a reference elastic hash implementation.

---

## Appendix: Proof of Correctness for Energy-Based Probing

### Problem Statement

The Elastic Hashing paper requires a bijection $\phi: \mathbb{Z}^+ \times \mathbb{Z}^+ \to \mathbb{Z}^+$ such that:
$$ \phi(i, j) \le O(i \cdot j^2) $$

We define our **Energy-Based** traversal using the cost function $E(i, j) = i \cdot j^2$. We visit pairs in increasing order of $E$.

### Proof

Let $K = E(i, j) = i \cdot j^2$. The rank $\psi(i, j)$ of a probe is bounded by the number of pairs $(x, y)$ with $E(x, y) \le K$:

$$ N(K) = |\{(x, y) : x \cdot y^2 \le K, x \ge 1, y \ge 1\}| $$

For a fixed $y \ge 1$, the max value of $x$ is $\lfloor K/y^2 \rfloor$. Summing over all valid $y$ (where $y^2 \le K$):

$$ N(K) = \sum_{y=1}^{\lfloor\sqrt{K}\rfloor} \lfloor \frac{K}{y^2} \rfloor \le K \sum_{y=1}^{\lfloor\sqrt{K}\rfloor} \frac{1}{y^2} $$

The sum $\sum_{y=1}^{\infty} \frac{1}{y^2}$ converges to $\frac{\pi^2}{6}$. Thus:

$$ N(K) < K \cdot \frac{\pi^2}{6} \approx 1.645 \cdot K $$

Substituting $K = i \cdot j^2$, we get:

$$ \psi(i, j) \le 1.645 \cdot (i \cdot j^2) = O(i \cdot j^2) $$

This confirms that our traversal order satisfies the algorithm's complexity requirements.
