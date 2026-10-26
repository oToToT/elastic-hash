// Dependency-free, reproducible lookup benchmark. Override ELASTIC_HASH_HEADER
// with an absolute path to measure a saved baseline using the same workload.
#ifndef ELASTIC_HASH_HEADER
#define ELASTIC_HASH_HEADER "elastic_hash.hpp"
#endif
#include ELASTIC_HASH_HEADER
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
  std::cout << "size,operation,median_ns_per_operation,checksum\n";
  for (int n : {1024, 8192, 65536}) {
    std::vector<double> insert_times;
    std::uint64_t insert_sum = 0;
    for (int sample = 0; sample < 7; ++sample) {
      ElasticHashTable<int, int> inserted;
      const auto start = std::chrono::steady_clock::now();
      for (int i = 0; i < n; ++i)
        inserted.insert({i, i});
      insert_times.push_back(std::chrono::duration<double, std::nano>(
                                 std::chrono::steady_clock::now() - start)
                                 .count() /
                             n);
      for (const auto &item : inserted)
        insert_sum += item.second;
    }
    std::sort(insert_times.begin(), insert_times.end());
    std::cout << n << ",insert," << insert_times[3] << ',' << insert_sum
              << '\n';
    ElasticHashTable<int, int> table;
    for (int i = 0; i < n; ++i)
      table.insert({i, i});
    for (bool miss : {false, true}) {
      std::vector<double> times;
      std::uint64_t sum = 0;
      for (int sample = 0; sample < 7; ++sample) {
        const auto start = std::chrono::steady_clock::now();
        for (int r = 0; r < 20; ++r) {
          for (int i = 0; i < n; ++i) {
            auto it = table.find(i + (miss ? n : 0));
            if (it != table.end())
              sum += it->second;
          }
        }
        times.push_back(std::chrono::duration<double, std::nano>(
                            std::chrono::steady_clock::now() - start)
                            .count() /
                        (20 * n));
      }
      std::sort(times.begin(), times.end());
      std::cout << n << ',' << (miss ? "miss" : "hit") << ',' << times[3] << ','
                << sum << '\n';
    }
  }
}
