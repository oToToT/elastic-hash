// Compile against a header with the debug_probes counter inserted by
// profile_probes.py. These are operation counts, not timing measurements.
#include "elastic_hash.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

constexpr std::uint64_t seed = 0x519b6f32d4817a09ULL;
std::uint64_t mix(std::uint64_t x) {
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  return x ^ (x >> 31);
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  const auto n = std::stoull(argv[1]);
  ElasticHashTable<std::uint64_t, std::uint64_t> table;
  std::vector<std::size_t> order(n), keys;
  std::iota(order.begin(), order.end(), 0);
  std::mt19937_64 random(seed);
  std::shuffle(order.begin(), order.end(), random);
  keys.reserve(n);
  for (auto i : order)
    keys.push_back(mix(seed + i));
  for (std::size_t i = 0; i < n; ++i)
    table.insert({keys[i], i + 1});
  std::iota(order.begin(), order.end(), 0);
  std::shuffle(order.begin(), order.end(), random);
  std::cout << "size,load,operation,sampled_queries,mean_probes,p50,p95,p99,"
               "max,one_probe_percent\n";
  for (bool miss : {false, true}) {
    if (miss)
      std::shuffle(order.begin(), order.end(), random);
    std::vector<std::uint64_t> probes;
    for (std::size_t j = 0; j < std::min<std::size_t>(100000, n); ++j) {
      const auto key = miss ? mix(seed + order[j] + n) : keys[order[j]];
      elastic_hash_detail::debug_probes = 0;
      auto found = table.find(key);
      if (miss ? found != table.end()
               : found == table.end() || found->second != order[j] + 1)
        return 1;
      probes.push_back(elastic_hash_detail::debug_probes);
    }
    std::sort(probes.begin(), probes.end());
    double mean = static_cast<double>(std::accumulate(
                      probes.begin(), probes.end(), std::uint64_t{0})) /
                  probes.size();
    auto one = std::count(probes.begin(), probes.end(), 1);
    std::cout << n << ',' << table.load_factor() << ','
              << (miss ? "miss" : "hit") << ',' << probes.size() << ',' << mean
              << ',' << probes[probes.size() / 2] << ','
              << probes[95 * probes.size() / 100] << ','
              << probes[99 * probes.size() / 100] << ',' << probes.back() << ','
              << 100.0 * one / probes.size() << '\n';
  }
}
