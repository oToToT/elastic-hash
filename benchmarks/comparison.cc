// Linux/glibc benchmark harness. Throughput has no allocation instrumentation.
// Each invocation runs one map/key/hash/size configuration in its own process.
#include "absl/container/flat_hash_map.h"
#include "ankerl/unordered_dense.h"
#ifndef ELASTIC_HASH_HEADER
#define ELASTIC_HASH_HEADER "elastic_hash.hpp"
#endif
#include ELASTIC_HASH_HEADER
#include <ext/pb_ds/assoc_container.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <malloc.h>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {
using Value = std::uint64_t;
using Clock = std::chrono::steady_clock;
constexpr std::uint64_t seed = 0x519b6f32d4817a09ULL;

std::uint64_t mix(std::uint64_t x) {
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  return x ^ (x >> 31);
}
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
// A memory clobber keeps passes observable and prevents loop hoisting/CSE.
void barrier(const void *p) { asm volatile("" : : "g"(p) : "memory"); }

struct Config {
  std::string map, key_type, hash_mode;
  std::size_t count;
  int samples;
  double target_ms;
  std::vector<std::string> operations;
  float target_load = 0;
  bool enabled(std::string_view operation) const {
    return operations.empty() || std::find(operations.begin(), operations.end(),
                                           operation) != operations.end();
  }
};

template <class Key> struct SharedHash {
  using is_avalanching = void;
  std::size_t operator()(const Key &key) const noexcept {
    return ankerl::unordered_dense::hash<Key>{}(key);
  }
};
// A bijective integer mixer gives provably distinct inserted and missing keys.
// String keys are 24 hexadecimal characters, exceeding libstdc++'s SSO limit.
template <class Key> Key make_key(std::size_t index) {
  auto x = mix(seed + index);
  if constexpr (std::is_same_v<Key, std::uint64_t>) {
    return x;
  } else {
    const auto y = mix(x);
    constexpr char digits[] = "0123456789abcdef";
    std::string result(24, '0');
    for (unsigned i = 0; i < 16; ++i)
      result[i] = digits[(x >> (4 * i)) & 15];
    for (unsigned i = 0; i < 8; ++i)
      result[i + 16] = digits[(y >> (4 * i)) & 15];
    return result;
  }
}

template <class Key> struct Dataset {
  std::vector<Key> keys, hits, misses, mixed;
  Value expected;
  explicit Dataset(std::size_t n) : expected(n * (n + 1) / 2) {
    keys.reserve(n);
    hits.reserve(n);
    misses.reserve(n);
    mixed.reserve(2 * n);
    std::vector<std::size_t> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::mt19937_64 random(seed);
    std::shuffle(order.begin(), order.end(), random);
    for (auto i : order)
      keys.push_back(make_key<Key>(i));
    order.resize(n);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), random);
    for (auto i : order)
      hits.push_back(keys[i]);
    std::shuffle(order.begin(), order.end(), random);
    for (auto i : order)
      misses.push_back(make_key<Key>(i + n));
    for (std::size_t i = 0; i < n; ++i) {
      mixed.push_back(hits[i]);
      mixed.push_back(misses[i]);
    }
  }
};

template <class Map> void reserve(Map &map, std::size_t n) {
  if constexpr (requires { map.reserve(n); })
    map.reserve(n);
  else
    map.resize(2 * n); // PBDS default load policy grows at 0.5 occupancy.
}
// Fixed-occupancy runs preallocate before timing. Libraries may honor the
// requested maximum differently; the final bucket count is always reported.
template <class Map>
void prepare(Map &map, const Config &c, bool reserved = false) {
  if (c.target_load > 0) {
    if constexpr (requires { map.max_load_factor(c.target_load); }) {
      map.max_load_factor(c.target_load);
      reserve(map, c.count);
    } else {
      throw std::invalid_argument("table has no configurable max_load_factor");
    }
  } else if (reserved) {
    reserve(map, c.count);
  }
}
template <class Map> float configured_load(const Map &map) {
  if constexpr (requires { map.max_load_factor(); })
    return map.max_load_factor();
  else
    return -1;
}
template <class Map> std::size_t buckets(const Map &map) {
  if constexpr (requires { map.bucket_count(); })
    return map.bucket_count();
  else
    return map.get_resize_policy().get_actual_size();
}
template <class Map, class Key> void fill(Map &map, const Dataset<Key> &data) {
  for (std::size_t i = 0; i < data.keys.size(); ++i)
    map.insert({data.keys[i], i + 1});
}
template <class Map> Value sum_values(const Map &map) {
  Value sum = 0;
  for (const auto &item : map)
    sum += item.second;
  return sum;
}
template <class Map, class Key>
void validate(const Map &map, const Dataset<Key> &data) {
  require(map.size() == data.keys.size(), "wrong table size");
  require(sum_values(map) == data.expected, "wrong table checksum");
  for (std::size_t i = 0; i < data.keys.size(); ++i) {
    auto it = map.find(data.keys[i]);
    require(it != map.end() && it->second == i + 1,
            "incorrect key/value lookup");
  }
}

template <class Map>
void row(const Config &c, std::string_view operation, int sample, double ns,
         Value checksum, const Map &map, double heap_bytes = -1) {
  const auto capacity = buckets(map);
  std::cout << c.map << ',' << c.key_type << ',' << c.hash_mode << ','
            << c.count << ',' << operation << ',' << sample << ','
            << std::setprecision(10) << ns << ',' << checksum << ',' << capacity
            << ',' << static_cast<double>(map.size()) / capacity << ','
            << heap_bytes << ',' << c.target_load << ',' << configured_load(map)
            << '\n';
}

template <class Map, class Key>
void insertion(const Config &c, const Dataset<Key> &data, bool reserved) {
  // An untimed first sample warms allocator/code paths equally for all maps.
  for (int sample = -1; sample < c.samples; ++sample) {
    Map map;
    prepare(map, c, reserved);
    const auto initial_buckets = buckets(map);
    barrier(&map);
    auto start = Clock::now();
    fill(map, data);
    barrier(&map);
    double ns =
        std::chrono::duration<double, std::nano>(Clock::now() - start).count();
    require(map.size() == c.count && sum_values(map) == data.expected,
            "incorrect insertion result");
    if (c.target_load > 0)
      require(buckets(map) == initial_buckets,
              "preallocated table grew during insertion");
    if (sample >= 0)
      row(c, reserved ? "insert_reserved" : "insert_grow", sample, ns / c.count,
          data.expected, map);
  }
}

template <class Map, class Key, class Function>
void read_operation(const Config &c, const Dataset<Key> &data, const Map &map,
                    std::string_view name, std::size_t operations,
                    Value expected, Function function) {
  auto pass = [&] {
    barrier(&map);
    auto sum = function();
    barrier(&map);
    return sum;
  };
  auto start = Clock::now();
  require(pass() == expected, "incorrect pilot checksum");
  double pilot =
      std::chrono::duration<double, std::nano>(Clock::now() - start).count();
  auto repetitions = static_cast<std::size_t>(
      std::clamp(c.target_ms * 1e6 / std::max(1.0, pilot), 1.0, 10000.0));
  for (int sample = 0; sample < c.samples; ++sample) {
    Value checksum = 0;
    start = Clock::now();
    for (std::size_t i = 0; i < repetitions; ++i)
      checksum += pass();
    double ns =
        std::chrono::duration<double, std::nano>(Clock::now() - start).count();
    require(checksum == expected * repetitions, "incorrect timed checksum");
    row(c, name, sample, ns / (operations * repetitions), checksum, map);
  }
  (void)data;
}

template <class Map, class Key>
void mutation(const Config &c, const Dataset<Key> &data, bool churn) {
  for (int sample = -1; sample < c.samples; ++sample) {
    Map map;
    prepare(map, c);
    fill(map, data); // Construction and refill are outside the timed region.
    barrier(&map);
    auto start = Clock::now();
    for (std::size_t i = 0; i < c.count; ++i) {
      map.erase(data.hits[i]);
      if (churn)
        map.insert({data.misses[i], i + 1});
    }
    barrier(&map);
    double ns =
        std::chrono::duration<double, std::nano>(Clock::now() - start).count();
    if (churn)
      require(map.size() == c.count && sum_values(map) == data.expected,
              "incorrect churn result");
    else
      require(map.empty(), "incorrect erase result");
    if (sample >= 0)
      row(c, churn ? "churn" : "erase", sample,
          ns / (c.count * (churn ? 2 : 1)), churn ? data.expected : 0, map);
  }
}

std::uint64_t allocated_bytes() {
  auto info = mallinfo2();
  return info.uordblks + info.hblkhd;
}
template <class Map, class Key>
void run(const Config &c, const Dataset<Key> &data) {
  // This runs before throughput warmups. Input keys/queries already exist.
  // A separate process per configuration avoids previous-map allocator state.
  const auto before = allocated_bytes();
  {
    Map map;
    prepare(map, c);
    fill(map, data);
    const auto after = allocated_bytes();
    require(after >= before, "allocator counter went backwards");
    row(c, "memory", 0, -1, 0, map,
        static_cast<double>(after - before) / c.count);
    validate(map, data);
  }
  if (c.enabled("insert_grow"))
    insertion<Map>(c, data, false);
  if (c.enabled("insert_reserved"))
    insertion<Map>(c, data, true);
  if (c.enabled("hit") || c.enabled("miss") || c.enabled("mixed") ||
      c.enabled("iterate")) {
    Map map;
    prepare(map, c);
    fill(map, data);
    validate(map, data);
    auto lookup = [&](const std::vector<Key> &queries) {
      Value sum = 0;
      for (const auto &key : queries) {
        auto it = map.find(key);
        if (it != map.end())
          sum += it->second;
      }
      return sum;
    };
    if (c.enabled("hit"))
      read_operation(c, data, map, "hit", c.count, data.expected,
                     [&] { return lookup(data.hits); });
    if (c.enabled("miss"))
      read_operation(c, data, map, "miss", c.count, 0,
                     [&] { return lookup(data.misses); });
    if (c.enabled("mixed"))
      read_operation(c, data, map, "mixed", 2 * c.count, data.expected,
                     [&] { return lookup(data.mixed); });
    if (c.enabled("iterate"))
      read_operation(c, data, map, "iterate", c.count, data.expected,
                     [&] { return sum_values(map); });
  }
  if (c.enabled("erase"))
    mutation<Map>(c, data, false);
  if (c.enabled("churn"))
    mutation<Map>(c, data, true);
}

using Resize = __gnu_pbds::hash_standard_resize_policy<
    __gnu_pbds::hash_exponential_size_policy<>,
    __gnu_pbds::hash_load_check_resize_trigger<>, true>;

template <class Key, bool Shared> void select_map(const Config &c) {
  Dataset<Key> data(c.count);
  using StdHash = std::conditional_t<Shared, SharedHash<Key>, std::hash<Key>>;
  using DenseHash = std::conditional_t<Shared, SharedHash<Key>,
                                       ankerl::unordered_dense::hash<Key>>;
  using AbslHash = std::conditional_t<Shared, SharedHash<Key>, absl::Hash<Key>>;
  if (c.map == "elastic")
    run<ElasticHashTable<Key, Value, StdHash>>(c, data);
  else if (c.map == "std")
    run<std::unordered_map<Key, Value, StdHash>>(c, data);
  else if (c.map == "dense")
    run<ankerl::unordered_dense::map<Key, Value, DenseHash>>(c, data);
  else if (c.map == "absl")
    run<absl::flat_hash_map<Key, Value, AbslHash>>(c, data);
  else if (c.map == "gnu_gp")
    run<__gnu_pbds::gp_hash_table<Key, Value, StdHash, std::equal_to<Key>,
                                  __gnu_pbds::direct_mask_range_hashing<>,
                                  __gnu_pbds::linear_probe_fn<>, Resize>>(c,
                                                                          data);
  else if (c.map == "gnu_cc")
    run<__gnu_pbds::cc_hash_table<Key, Value, StdHash, std::equal_to<Key>,
                                  __gnu_pbds::direct_mask_range_hashing<>,
                                  Resize>>(c, data);
  else
    throw std::invalid_argument("unknown map");
}
} // namespace

int main(int argc, char **argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--info") {
    std::cout << "compiler: " << __VERSION__ << "\nlibstdc++: " << __GLIBCXX__
              << '\n';
    return 0;
  }
  if (argc != 7 && argc != 8 && argc != 9) {
    std::cerr
        << "usage: hash_comparison MAP u64|string native|shared SIZE "
           "SAMPLES TARGET_MS [comma-separated-operations] [target-load]\n";
    return 2;
  }
  try {
    Config c{argv[1],
             argv[2],
             argv[3],
             std::stoull(argv[4]),
             std::stoi(argv[5]),
             std::stod(argv[6]),
             {}};
    if (argc >= 8) {
      std::string operations = argv[7];
      std::size_t begin = 0;
      while (begin < operations.size()) {
        const auto end = operations.find(',', begin);
        auto op = operations.substr(
            begin, end == std::string::npos ? end : end - begin);
        require(op == "insert_grow" || op == "insert_reserved" || op == "hit" ||
                    op == "miss" || op == "mixed" || op == "iterate" ||
                    op == "erase" || op == "churn",
                "unknown operation");
        c.operations.push_back(std::move(op));
        if (end == std::string::npos)
          break;
        begin = end + 1;
      }
      require(!c.operations.empty(), "empty operation selection");
    }
    if (argc == 9)
      c.target_load = std::stof(argv[8]);
    require(std::isfinite(c.target_load) && c.target_load >= 0 &&
                c.target_load < 1,
            "target load must be zero (default policy) or in (0, 1)");
    if (c.target_load > 0)
      require(!c.enabled("insert_grow"),
              "fixed occupancy uses insert_reserved, not insert_grow");
    require(c.count > 0 && c.count <= 10000000 && c.samples > 0 &&
                c.samples <= 100 && c.target_ms > 0 && c.target_ms <= 1000,
            "invalid benchmark settings");
    require(c.hash_mode == "native" || c.hash_mode == "shared",
            "unknown hash mode");
    std::cout << "map,key_type,hash,size,operation,sample,ns_per_op,checksum,"
                 "buckets,load_factor,heap_bytes_per_entry,target_load,"
                 "configured_max_load\n";
    if (c.key_type == "u64") {
      if (c.hash_mode == "shared")
        select_map<std::uint64_t, true>(c);
      else
        select_map<std::uint64_t, false>(c);
    } else if (c.key_type == "string") {
      if (c.hash_mode == "shared")
        select_map<std::string, true>(c);
      else
        select_map<std::string, false>(c);
    } else
      throw std::invalid_argument("unknown key type");
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
